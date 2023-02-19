///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2005, 2006, 2007, 2008, 2009
// University of Rochester
// Department of Computer Science
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the University of Rochester nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.


//=============================================================================
// Non-blocking Object-Based STM (RSTM)
//=============================================================================
//   This is the original RSTM algorithm, which is object-based and
//   non-blocking via cloning. There is support for invisible and visible
//   readers, as well as eager and lazy write acquisition.
//
//   Various forms of inevitability and privatization are available, as well as
//   multiple contention managers.
//=============================================================================

#ifndef RSTM_HPP
#define RSTM_HPP

#include <string>
#include "support/defs.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/MiniVector.hpp"
#include "support/ConflictDetector.hpp"
#include "support/MMPolicy.hpp"
#include "support/Inevitability.hpp"
#include "cm/ContentionManager.hpp"
#include "cm/CMPolicies.hpp"
#include "support/TokenManager.hpp"
#include "support/atomic_ops.h"
#include "support/Retry.hpp"

#if defined(STM_ROLLBACK_SETJMP)
#include <setjmp.h>
#endif

namespace rstm {

class Descriptor;
struct SharedHandle;

/***  Each thread needs a thread-local pointer to its Descriptor */
extern stm::ThreadLocalPointer<Descriptor>currentDescriptor;

#ifdef STM_PRIV_NONBLOCKING
/**
 *  if we're using nonblocking privatization, we need a flag to trigger
 *  immediate validation in active transactions whenever a privatizer commits.
 */
extern volatile unsigned long privatizer_clock;
#endif

extern stm::TokenManager<Descriptor> readbits;

/**
 *  Object provides the interface and metadata that a transaction needs to
 *  detect and resolve conflicts, and to create and destroy clones.  Object
 *  inherits from CustomAllocedBase so that it gets access to whatever custom
 *  allocation strategy is currently in place.
 */
class Object : public stm::mm::CustomAllocedBase
{
  friend class Descriptor;
  friend Object* open_privatized(SharedHandle* const);
 protected:
  /**
   *  Version from which /this/ was cloned.  The bits of this field are
   *  volatile, so that we can zero them out when a transaction commits, and be
   *  sure that other threads immediately see that the bits are zeroed.
   */
  Object* volatile m_next;

  /**
   *  The tx who created /this/.  Once this is set, it never changes, but we
   *  can't actually make this const because of the convoluted path for object
   *  construction.
   */
  Descriptor* m_owner;

  /**
   *  The header through which /this/ is accessed.  m_st is immutable; once it
   *  is set it will never change.
   */
  SharedHandle* m_st;

  /**
   *  Ctor: zeros out all fields.
   */
  Object() : m_next(NULL), m_owner(NULL), m_st(NULL) { }

};

////////////////////////////////////////////
// Utilities for managing object metadata //
////////////////////////////////////////////

/***  Check if the object is dirty, i.e. its lsb == 1. */
inline bool is_owned(const Object* const header) {
  return reinterpret_cast<unsigned long>(header) & 0x1;
}

/***  Zero the lsb of an Object* */
inline Object* get_data_ptr(const Object* const header) {
  return reinterpret_cast<Object*>
    (reinterpret_cast<unsigned long>(header) & (~1));
}

/***  Turn on the lsb of an Object*. */
inline Object* set_lsb(const Object* const curr_data) {
  return reinterpret_cast<Object*>
    (reinterpret_cast<unsigned long>(curr_data) | 0x1);
}

/***  Turn off the lsb of an Object*. */
inline Object* unset_lsb(const Object* const curr_data) {
  return reinterpret_cast<Object*>
    (reinterpret_cast<unsigned long>(curr_data) & (~1));
}

/**
 * SharedHandle is the library class that manages access to shared
 * objects. SharedHandle is never seen by client code. The API manages these
 * handles, opening them for reading or writing as needed.
 *
 * Because the name "SharedHandle" never escapes through the API, and lots of
 * library code needs access to the handle's internals, I've made it a struct.
 *
 * Note that SharedHandle inherits from CustomAllocedBase, and thus uses the
 * 'malloc' and 'free' in namespace stm::mm that were specified at compile
 * time, with a wrapper to make those calls tx-safe.
 */
struct SharedHandle : public stm::mm::CustomAllocedBase
{
  /**
   *  Pointer to the shared data.  The lsb of m_payload is set if /this/ has
   *  been acquired by a writer and the writer has not cleaned up yet.
   *
   *  NB: __attribute__ used to avoid warning about strict aliasing rules when
   *      we cast this to an unsigned long in order to pass it to CAS
   *
   *  NB: the 'volatile' keyword is placed so that the actual bits of this
   *      field are volatile, not the bits on the other side of the pointer.
   *      This is safe because the only fields that one might look at in the
   *      referenced Object are already volatile.
   */
  Object* volatile __attribute__((__may_alias__)) m_payload;

  /**
   *  Visible reader bitmap.  Up to 32 transactions can read an object visibly
   *  by getting permission to 'own' one of the 32 global bits and then setting
   *  that bit in a shared object's visible reader bitmap.
   */
  volatile unsigned long m_readers;

  /**
   *  Similar to above, but for retry() (depending on retry implementation)
   */
  volatile stm::RetryMechanism::PerObjectRetryMetadata m_retryers;

  /**
   *  Constructor for shared objects. All this constructor does is initialize a
   *  SharedHandle to wrap the object for transactional use.
   *
   *  @param t - The Object that this shared manages.  t should not be NULL
   */
  SharedHandle(Object* t) : m_payload(t), m_readers(0), m_retryers(0) { }
}; // class rstm::SharedHandle


/***  Tuple for storing all the info we need in a lazy write set */
struct lazy_bookkeep_t {
  SharedHandle* shared;
  Object*     read_version;
  Object*     write_version;
  bool        isAcquired;

  lazy_bookkeep_t(SharedHandle* _sh = NULL, Object* _rd = NULL,
                  Object* _wr = NULL, bool _acq = false)
    : shared(_sh), read_version(_rd), write_version(_wr),
      isAcquired(_acq)
  { }
};

/***  Tuple for storing all the info we need in an eager write set */
struct eager_bookkeep_t {
  SharedHandle* shared;
  Object*     read_version;
  Object*     write_version;

  eager_bookkeep_t(SharedHandle* _sh = NULL, Object* _rd = NULL,
                   Object* _wr = NULL)
    : shared(_sh), read_version(_rd), write_version(_wr)
  { }
};

/***  Tuple for storing all the info we need in an invisible read set */
struct invis_bookkeep_t {
  SharedHandle* shared;
  Object*     read_version;

  invis_bookkeep_t(SharedHandle* _sh = NULL, Object* _rd = NULL)
    : shared(_sh), read_version(_rd)
  { }
};

/**
 *  The entire RSTM algorithm, and all of the metadata pertaining to an active
 *  transaction instance, are ensapsulated in the Descriptor.
 */
class Descriptor {
 public:
  /**
   * state is stm::COMMITTED, stm::ABORTED, or stm::ACTIVE
   */
  volatile unsigned long tx_state;

 private:
  /**
   *  sw vis reader bitmask.  If we get permission to use SharedHandle
   *  m_readers bits from the TokenManager, then this field will be nonzero.
   */
  unsigned long id_mask;

#ifdef STM_PRIV_NONBLOCKING
  /**
   *  If we're using nonblocking privatization, we need a copy of the value of
   *  the privatizer clock as of the last time we checked it.
   */
  unsigned long privatizer_clock_cache;
#endif

 public:
  /**
   * Policy wrapper around a CM
   */
  stm::HybridCMPolicy cm;

 private:
  /**
   * Retry support
   */
  static stm::RetryMechanism retryImpl;
  stm::RetryMechanism::RetryHandle* retryHandle;

 public:
  /**
   *  For privatization: check privatizer clock which will trigger validation
   *                     if necessary.
   */
  void check_privatizer_clock();

  /**
   *  Constructor is very straightforward
   */
  Descriptor(std::string dynamic_cm,std::string validation,bool use_static_cm);
  ~Descriptor();

  /***  Inevitability support */
  stm::InevPolicy inev;
  bool try_inevitable() {
    // handle duplicate calls before doing assertion below
    if (inev.isInevitable())
      return true;
    // currently, we only support becoming inevitable before performing
    // any transactional reads or writes
    assert((invisibleReads.size() == 0) && (visibleReads.size() == 0)
           && (eagerWrites.size() == 0) && (lazyWrites.size() == 0));
    return inev.try_inevitable();
  }

 private:
  /*** are we using lazy or eager acquire? */
  bool isLazy;

  /*** are we using visible or invisible reads? */
  bool isVisible;

 public:
  /**
   *  Interface to thread-local allocator that manages reclamation on
   *  abort / commit automatically.
   */
  stm::RADDMMPolicy mm;

 public:
  /**
   *  Set all metadata up for a new transaction, and move to stm::ACTIVE
   */
#if defined(STM_ROLLBACK_SETJMP)
  void begin_transaction(jmp_buf* buf)
#else
  void begin_transaction()
#endif
  {
    // only for outermost transaction
    if (nesting_depth == 0) {
      inev.onBeginTx();
      conflicts.onTxBegin();
      mm.onTxBegin();

      // mark myself active
      tx_state = stm::ACTIVE;

#if defined(STM_ROLLBACK_SETJMP)
      setjmp_buf = buf;
#endif

      // cm notification
      cm.onBeginTx();
    }

    nesting_depth++;
  }

  /**
   *  Attempt to commit a transaction
   */
  void commit();

 private:
  /**
   * Undo a transaction's operations and roll back by throwing Rollback()
   */
  void abort();

 public:
  /**
   *  Clean up after a transaction, return true if the tx is stm::COMMITTED.
   *  This cleans up all the metadata within the Descriptor, and also cleans up
   *  any per-object metadata that the most recent transaction modified.
   */
  void rollback();

  /*** Called by the catch block in END_TRANSACTION to implement retry(). */
  void retry();

 private:
  /***  The current depth of nested transactions, where 0 is no transaction. */
  unsigned int nesting_depth;

#if defined(STM_ROLLBACK_SETJMP)
  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;
#endif

  /**
   * explicit validation of a transaction's state: make sure tx_state isn't
   * stm::ABORTED
   */
  void verifySelf() {
    if (tx_state == stm::ABORTED)
      abort();
  }

  /**
   *  If we're using conflict detection heuristic to avoid validation, this
   *  stores our local heuristic information.
   */
  stm::ValidationPolicy conflicts;

  /**
   *  Make sure that any object we read and are writing lazily hasn't changed.
   */
  void verifyLazyWrites();

  /**
   *  Make sure that any object we read and aren't writing hasn't changed.
   */
  void verifyInvisReads();

  /***  Acquire all objects on the lazy write list */
  void acquireLazily();

  /**
   *  clean up all objects in the eager write set.  If we committed, then we
   *  should unset the lsb of each object's header.  If we aborted, we should
   *  swap the pointer of each header from the clone to the original version.
   */
  void cleanupEagerWrites(unsigned long tx_state);

  /**
   *  Lookup an entry in the lazy write set.  If we try to read something that
   *  we've opened lazily, the only way to avoid an alias error is to use this
   *  method.
   */
  Object* lookupLazyWrite(SharedHandle* shared) const;

  /**
   *  clean up all objects in the lazy write set.  If we committed, then we
   *  should unset the lsb of each object's header.  If we aborted, then for
   *  each object that we successfully acquired, we should swap the pointer of
   *  the header from the clone to the original version.
   */
  void cleanupLazyWrites(unsigned long tx_state);

  /**
   *  Combine add / lookup / validate in the read set.  This lets me avoid
   *  duplicate entries.  If I'm opening O, instead of looking up O in my read
   *  set I just assume it's new, open it, and then use a validating add.  For
   *  each object in the set, I validate the object.  Then, if that object is
   *  O, I can quit early.  Otherwise, once I'm done traversing the list I add
   *  O, and I've both validated and ensured no duplicates.
   */
  void addValidateInvisRead(SharedHandle* shared, Object* version);

  /***  Validate the invisible read and lazy write sets */
  void validate();

  /**
   *  Validate the invisible read set and insert a new entry if it isn't
   *  already in the set.
   */
  void validatingInsert(SharedHandle* shared, Object* version);

  /**
   *  Find an element in the read set and remove it.  We use this for early
   *  release, but we don't use it for upgrading reads to writes.
   */
  void removeInvisRead(const SharedHandle* const shared);

  /**
   *  For each entry in the visible read set, take myself out of the object's
   *  reader list.
   */
  void cleanupVisReads();

  /**
   *  Remove an entry from the visible reader list, as part of early release.
   */
  void removeVisRead(const SharedHandle* const shared);

  /***  Bookkeeping typedefs */
  typedef stm::MiniVector<invis_bookkeep_t> InvisReadLog;
  typedef stm::MiniVector<SharedHandle*>    VisReadLog;
  typedef stm::MiniVector<eager_bookkeep_t> EagerWriteLog;
  typedef stm::MiniVector<lazy_bookkeep_t>  LazyWriteLog;

  InvisReadLog  invisibleReads;     /// Invisible read log
  VisReadLog    visibleReads;       /// Visible read log
  EagerWriteLog eagerWrites;        /// Eager write log
  LazyWriteLog  lazyWrites;         /// Lazy write log

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;

 public:
  unsigned getCommits() { return num_commits; }
  unsigned getAborts()  { return num_aborts; }
  unsigned getRetrys()  { return num_retrys; }

  /**
   *  Ensure that Object t has a shared header guarding access to it
   *
   *  For now, with stm::Object caching a SharedHandle* back-pointer (m_st), we
   *  can easily guarantee that any Object will have one and only one header
   *  wrapping it.  If we remove the back pointer, we can no longer provide
   *  this guarantee from within the runtime.  Instead, it will become the
   *  programmer's responsibility to ensure that an object is never passed to
   *  this method more than once during its lifetime.
   *
   *  @param t - an object that needs a shared header
   *
   *  @returns - a pointer to the shared header with which /this/ is
   *  associated.
   */
  static SharedHandle* CreateShared(Object* t) {
    // we don't allow a header to point to NULL.  Instead, if the payload is
    // null, we have a null header, too.
    if (NULL == t)
      return NULL;

    // only set up a new header if the current back-pointer is null
    if (NULL == t->m_st) {
      // create the header object
      t->m_st = new SharedHandle(t);
      // NB: Object() ctor already nulled out m_next and m_owner
    }

    return t->m_st;
  }

  /**
   *  Used in various places for a thread to get access to its thread local
   *  Descriptor. This is required for some generic programming that we do,
   *  where the client code is written to be cross-library compatible.
   *
   *  The reason that this is an issue is that each library has its own
   *  currentDescriptor, which a client that isn't library-specific shouldn't
   *  know about.
   *
   *  It's redundant from the perspective of any code that actually imports the
   *  library-specific namespace.
   */
  static Descriptor& MyDescriptor() { return *currentDescriptor; }

  /**
   *  It's never correct to destroy an object inside of a transaction.
   *  Instead, use this method to schedule an object for destruction upon
   *  commit.
   *
   *  NB: The problem with in-tx destruction is that when g++ uses virtual
   *      destructors, it modifies the vtable.  Vtable modifications can't be
   *      rolled back on abort, and can have bad side effects in other
   *      concurrent transactions.
   */
  void deleteShared(SharedHandle* sh)
  {
    mm.deleteOnCommit.insert(sh);
    mm.deleteOnCommit.insert(openReadOnly(sh));
  }

 private:
  /**
   *  Check if shared.header and expected_version agree on the currently valid
   *  version of the data.  Used to validate objects
   */
  bool isCurrent(const SharedHandle* header, const Object* expected) const;

  /***  Acquire an object when executing with lazy. */
  bool lazyAcquire(SharedHandle* header, const Object* expected,
                   const Object* newer);

 public:
  /***  Clean up an object's metadata when its owner transaction committed. */
  static bool CleanOnCommit(SharedHandle* header, Object* valid_ver);

  /***  Clean up an object's metadata when its owner aborted */
  static bool CleanOnAbort(SharedHandle* header, const Object* expect,
                           const Object* replacement);

 private:
  /***  remove this tx descriptor from the visible readers bitmap */
  void removeVisibleReader(SharedHandle* const header) const;

  /***  replace one header with another; thin wrapper around bool_cas */
  static bool SwapHeader(SharedHandle* header, Object* expected,
                         Object* replacement);

  /***  abort all visible readers embedded in the reader bitmap */
  void abortVisibleReaders(SharedHandle* header);

  /*** install this tx descriptor as a visible reader of an object */
  bool installVisibleReader(SharedHandle* header);

 public:
  bool inTransaction() const { return tx_state != stm::COMMITTED; }

  Object* openReadOnly(SharedHandle* const sh);

  Object* openReadWrite(SharedHandle* const sh, const size_t objsize);

  Object* upgradeToReadWrite(SharedHandle* const sh, const size_t objsize) {
    return openReadWrite(sh, objsize);
  }

#if defined(STM_ROLLBACK_THROW)
  /*** unwind the stack */
  __attribute__((flatten))
  void unwind() {
    // throw unless at outermost level
    if (--nesting_depth > 0)
      throw stm::RollBack();
  }
#endif

#if defined(STM_ROLLBACK_THROW)
  void onError() {
    if (nesting_depth-- > 1)
      return;
    tx_state = stm::ABORTED;
    rollback();
  }
#endif

  /**
   *  Heavyweight method to ensure that this transaction didn't open sh for
   *  writing and hold on to obj as a readable pointer.
   */
  bool isAliased(const SharedHandle* const sh, const Object* const obj) const;

  /***  Remove an object from the read set of this transaction. */
  void release(SharedHandle* const obj);

  void deleteTransactionally(SharedHandle* sh) {
    mm.deleteOnCommit.insert(sh);
    mm.deleteOnCommit.insert(const_cast<Object*>(openReadOnly(sh)));
  }
};  // class rstm::Descriptor

/*** wrapper to allocate memory */
inline void* tx_alloc(size_t size) {
  return currentDescriptor->mm.txAlloc(size);
}

/*** wrapper to free memory */
inline void tx_free(void* ptr) {
  Descriptor* tx = currentDescriptor.get();
  if (tx->tx_state == stm::COMMITTED)
    tx->mm.txFree(ptr);
  else
    tx->mm.deleteOnCommit.insert((stm::mm::CustomAllocedBase*)ptr);
}

inline void Descriptor::abort() {
  // need to assert not inevitable

  tx_state = stm::ABORTED;
  rollback();
#if defined(STM_ROLLBACK_THROW)
  throw stm::RollBack();
#elif defined(STM_ROLLBACK_SETJMP)
  nesting_depth = 0;
  longjmp(*setjmp_buf, 1);
#endif
}

inline void Descriptor::validate() {
  mm.onValidate();
  verifyInvisReads();
  verifyLazyWrites();
}

inline void Descriptor::removeVisRead(const SharedHandle* const shared) {
  assert(shared);
  for (VisReadLog::iterator i = visibleReads.begin(),
         e = visibleReads.end(); i != e; ++i) {
    if (shared == *i) {
      visibleReads.remove(i);
      return;
    }
  }
}

inline void Descriptor::removeInvisRead(const SharedHandle* const shared) {
  for (InvisReadLog::iterator i = invisibleReads.begin(),
         e = invisibleReads.end(); i != e; ++i) {
    if (i->shared == shared) {
      invisibleReads.remove(i);
      return;
    }
  }
}

inline Object* Descriptor::lookupLazyWrite(SharedHandle* shared) const {
  assert(shared);
  for (LazyWriteLog::iterator i = lazyWrites.begin(),
         e = lazyWrites.end(); i != e; ++i)
    if (i->shared == shared)
      return i->write_version;
  return NULL;
}

inline void Descriptor::check_privatizer_clock() {
#ifdef STM_PRIV_NONBLOCKING
  while (privatizer_clock_cache != privatizer_clock) {
    privatizer_clock_cache = privatizer_clock;
    validate();
    verifySelf();
  }
#endif
}

inline Descriptor::Descriptor(std::string dynamic_cm, std::string validation,
                              bool _use_static_cm)
  : id_mask(0),
    cm(_use_static_cm, dynamic_cm),         // set up CM
    retryHandle(new stm::RetryMechanism::RetryHandle()),
    mm(),                       // set up the DeferredReclamationMMPolicy
    conflicts(),                                // construct bookkeeping
    invisibleReads(64), visibleReads(64),       // fields that depend on
    eagerWrites(64), lazyWrites(64),            // the heap
    num_commits(0), num_aborts(0), num_retrys(0)
{
  // the state is stm::COMMITTED, in tx #0
  tx_state = stm::COMMITTED;

#ifdef STM_PRIV_NONBLOCKING
  privatizer_clock_cache = privatizer_clock;
#endif
  isVisible = false;
  // try to become visible... this should change eventually
  if (validation == "vis-eager" || validation == "vis-lazy") {
    int myToken = readbits.get_token(this);
    if (myToken != -1) {
      id_mask = 1 << myToken;
      isVisible = true;
    }
  }

  // for now, the acquire rule is boolean; 1=eager
  isLazy = !(validation == "invis-eager" || validation == "vis-eager");

  // set up retry handle
  retryImpl.init_thread(retryHandle);

  // initialize nesting depth
  nesting_depth = 0;
}

inline Descriptor::~Descriptor() {
  // we're leaking logs and such here if they aren't empty, but we assume that
  // the application is shutting down so we don't worry about it.
}

/**
 * @returns -- A bool indicating whether the transaction being rolled back is
 *             nested or not.  If it is nested, then we don't rollback, and we
 *             should rethrow the exception that caused us to land here.
 */
inline void Descriptor::rollback() {
  // we'd better be aborted if we're here
  assert(tx_state == stm::ABORTED);
  num_aborts++;

  // notify CM
  cm.onTxAborted();

  // at the end of a transaction, we are supposed to restore the headers of any
  // objects that we acquired
  cleanupLazyWrites(stm::ABORTED);
  cleanupEagerWrites(stm::ABORTED);

  // clean up read sets: uninstall myself from any objects I have open for
  // reading visibly, null out my invis read list
  cleanupVisReads();
  invisibleReads.reset();

  // commit memory changes and reset memory logging
  mm.onTxEnd(stm::ABORTED);

  // exit inevitability
  inev.onEndTx();
}

inline void Descriptor::commit() {
  // only try to commit if we are still stm::ACTIVE and at the end of the
  // outermost transaction
  if (nesting_depth > 1) {
    nesting_depth--;
    return;
  }

  if (tx_state != stm::ACTIVE)
    abort();

  // Contention Manager notification
  cm.onTryCommitTx();

  if (inev.isInevitable()) {
    // inevitable transaction doesn't need to do anything to its read and write
    // sets, but it must release the inevitability token
    tx_state = stm::COMMITTED;
    cm.onTxCommitted();
    inev.onInevCommit();
    retryImpl.onCommit(eagerWrites, lazyWrites);
  }
  else {
    // acquire objects that were open_RW'd lazily
    if (isLazy)
      acquireLazily();

    // validate if necessary
    if (!conflicts.tryCommit()) {
      verifyInvisReads();
      conflicts.forceCommit();
    }

    // cas status to commit; if this cas fails then I've been aborted
    if (!bool_cas(&(tx_state), stm::ACTIVE, stm::COMMITTED))
      abort();

    cm.onTxCommitted();
    retryImpl.onCommit(eagerWrites, lazyWrites);

    // at the end of a transaction, we are supposed to restore the headers of
    // any objects that we acquired (regardless of whether the tx aborted or
    // committed).
    cleanupLazyWrites(tx_state);
    cleanupEagerWrites(tx_state);

    // clean up read sets: uninstall myself from any objects I have open for
    // reading visibly, null out my invis read list
    cleanupVisReads();
    invisibleReads.reset();
  }

  // commit memory changes and reset memory logging
  mm.onTxEnd(stm::COMMITTED);

  // exit inevitability
  inev.onEndTx();

  ++num_commits;
  --nesting_depth;
}

// for each visible reader, we must remove the reader from the object once that
// is done, we can reset the vis_reads list
inline void Descriptor::cleanupVisReads() {
  for (VisReadLog::iterator i = visibleReads.begin(),
         e = visibleReads.end(); i != e; ++i)
    removeVisibleReader(*i);

  visibleReads.reset();
}

inline void Descriptor::verifyInvisReads() {
  for (InvisReadLog::iterator i = invisibleReads.begin(),
         e = invisibleReads.end(); i != e; ++i)
    if (!isCurrent(i->shared, i->read_version))
      abort();
}

inline void Descriptor::addValidateInvisRead(SharedHandle* shared,
                                             Object* version) {
  for (InvisReadLog::iterator i = invisibleReads.begin(),
         e = invisibleReads.end(); i != e; ++i)
  {
    if (!isCurrent(i->shared, i->read_version))
      abort();
    else if (i->shared == shared)
      return;
  }

  invisibleReads.insert(invis_bookkeep_t(shared, version));
}

inline void Descriptor::verifyLazyWrites() {
  // Leaving this in so that we don't need the wsync on empty. Normally we'd
  // just let the for loop deal with this.
  if (lazyWrites.is_empty())
    return;

  for (LazyWriteLog::iterator i = lazyWrites.begin(),
         e = lazyWrites.end(); i != e; ++i)
    if (!isCurrent(i->shared, i->read_version))
      abort();
}

// acquire all objects lazily opened for writing
inline void Descriptor::acquireLazily() {
  // Leave this in to short circuit when there's nothing to acquire. Normally
  // just let the for loop handle this.
  if (lazyWrites.is_empty())
    return;

  for (LazyWriteLog::iterator i = lazyWrites.begin(),
         e = lazyWrites.end(); i != e; ++i)
  {
    assert(!i->isAcquired);
    if (!lazyAcquire(i->shared, i->read_version, i->write_version))
      abort();
    i->isAcquired = true;
  }
}

// after COMMIT/ABORT, cleanup headers of all objects that we acquired/cloned
// lazily
inline void Descriptor::cleanupLazyWrites(unsigned long tx_state) {
  if (tx_state == stm::ABORTED) {
    for (LazyWriteLog::iterator i = lazyWrites.begin(),
           e = lazyWrites.end(); i != e; ++i)
      if (i->isAcquired)
        CleanOnAbort(i->shared, i->write_version, i->read_version);
  }
  else {
    assert(tx_state == stm::COMMITTED);
    for (LazyWriteLog::iterator i = lazyWrites.begin(),
           e = lazyWrites.end(); i != e; ++i)
      CleanOnCommit(i->shared, i->write_version);
  }
  lazyWrites.reset();
}

// after COMMIT/ABORT, cleanup headers of all objects that we acquired/cloned
// eagerly
inline void Descriptor::cleanupEagerWrites(unsigned long tx_state)
{
  if (tx_state == stm::ABORTED) {
    for (EagerWriteLog::iterator i = eagerWrites.begin(),
           e = eagerWrites.end(); i != e; ++i)
      CleanOnAbort(i->shared, i->write_version, i->read_version);
  }
  else {
    assert(tx_state == stm::COMMITTED);
    for (EagerWriteLog::iterator i = eagerWrites.begin(),
           e = eagerWrites.end(); i != e; ++i)
      CleanOnCommit(i->shared, i->write_version);
  }
  eagerWrites.reset();
}

inline bool Descriptor::isCurrent(const SharedHandle* header,
                                  const Object* expected) const
{
  Object* payload = const_cast<Object*>(header->m_payload);

  // if the header meets our expectations, validation succeeds
  if (payload == expected)
    return true;

  // the only other way that validation can succeed is if the owner
  // == tx and the installed object is a clone of expected
  Object* ver = get_data_ptr(payload);

  return ((ver->m_owner == this) && (ver->m_next == expected));
}

inline bool Descriptor::lazyAcquire(SharedHandle* header,
                                    const Object* expected,
                                    const Object* newer)
{
  // while loop to retry if cannot abort visible readers yet
  while (true) {
    // check if we can abort visible readers, restart loop on failure
    unsigned int index = 0;
    unsigned int bits = header->m_readers;
    bool canAbortAll = true;

    while (bits) {
      if (bits & 1) {
        Descriptor* reader = readbits.lookup(index);

        if (reader != this) {
          // if can't abort this reader, return false
          stm::ConflictResolutions r = cm.onWAR(reader->cm.getCM());
          if (r == stm::AbortSelf)
            abort();
          if (r != stm::AbortOther) {
            canAbortAll = false;
            break;
          }
        }
      }
      bits >>= 1;
      index++;
    }

    if (!canAbortAll) {
      cm.onContention();
      verifySelf();
      continue;
    }

    // try to acquire this SharedHandle, restart loop on failure
    if (!SwapHeader(header, unset_lsb(expected), set_lsb(newer)))
      return false;

    // successfully acquired this SharedHandle; now abort visible readers
    abortVisibleReaders(header);
    cm.onOpenWrite();
    return true;
  }
}

inline bool Descriptor::CleanOnCommit(SharedHandle* header, Object* valid_ver)
{
  // this used to just return the result of swapHeader, but since we
  // want to null out the next pointer we must be more complex
  return SwapHeader(header, set_lsb(valid_ver), unset_lsb(valid_ver));
}

inline bool Descriptor::CleanOnAbort(SharedHandle* header,
                                     const Object* expected,
                                     const Object* replacement)
{
  return SwapHeader(header, set_lsb(expected), unset_lsb(replacement));
}

inline void Descriptor::removeVisibleReader(SharedHandle* const header) const
{
  // don't bother with the CAS if we're not in the bitmap
  if (header->m_readers & id_mask) {
    unsigned long obj_mask;
    do {
      obj_mask = header->m_readers;
    }
    while (!bool_cas(&header->m_readers, obj_mask, obj_mask & (~id_mask)))
      /* intentionally empty, spinning on failure */;
  }
}


inline bool Descriptor::SwapHeader(SharedHandle* header, Object* expected,
                                   Object* replacement)
{
  return bool_cas(reinterpret_cast<volatile unsigned long*>
                  (&header->m_payload),
                  reinterpret_cast<unsigned long>(expected),
                  reinterpret_cast<unsigned long>(replacement));
}

inline bool Descriptor::installVisibleReader(SharedHandle* header) {
  if (header->m_readers & id_mask)
    return false;
  unsigned long obj_mask;
  do {
    obj_mask = header->m_readers;
  }
  while (!bool_cas(&header->m_readers, obj_mask, obj_mask | id_mask));
  return true;
}

inline void Descriptor::abortVisibleReaders(SharedHandle* header) {
  unsigned long tmp_reader_bitmap = header->m_readers;
  unsigned long desc_array_index = 0;

  while (tmp_reader_bitmap) {
    if (tmp_reader_bitmap & 1) {
      Descriptor* reader = readbits.lookup(desc_array_index);
      // abort only if the reader exists, is stm::ACTIVE, and isn't me
      if (reader && reader != this) {
        if (reader->tx_state == stm::ACTIVE)
          bool_cas(&reader->tx_state, stm::ACTIVE, stm::ABORTED);
      }
    }
    tmp_reader_bitmap >>= 1;
    desc_array_index++;
  }
}

inline void Descriptor::release(SharedHandle* const obj)
{
  // NB: no need to check if I own this in my eager or lazy writeset; that's
  // orthogonal to whether it's in my read set ... but that will change if lazy
  // writers can be visible

  // branch based on whether this is a visible read or not
  if ((obj->m_readers & id_mask) != 0) {
    // I'm a vis reader: remove self from the visible reader bitmap and remove
    // /this/ from my vis read set
    removeVisibleReader(obj);
    removeVisRead(obj);
  }
  else {
    // invis reader:  just remove /this/ from my invis read set
    removeInvisRead(obj);
  }
}

inline bool Descriptor::isAliased(const SharedHandle* const sh,
                                  const Object* const obj) const
{
  // if there's no sh, obviously we're done
  if (sh == NULL)
    return true;

  // read the header to a local.  if it is obj, we're good
  Object* orig = const_cast<Object*>(sh->m_payload);
  if (orig == obj)
    return true;

  // yikes!  they don't match.  this could mean a correct upgrade, it could
  // mean that we're destined to abort, or it could mean that we have an
  // upgrade error.

  // get the data pointer without the LSB
  Object* newer = get_data_ptr(orig);
  // if they match now, it means we upgraded correctly
  if (newer == obj)
    return true;
  // if orig didn't have a lsb set, then orig will equal newer.  This means
  // that someone else cleaned my write and aborted me, or else acquired my
  // read, made a change, committed, and cleaned up.  I'm destined to abort,
  // but I don't have an alias error
  if (newer == orig)
    return true;

  // if we're here, then newer's lsb was originally set, so there ought to be
  // an 'older'.  Get the 'older' as well as newer's owner
  Object* older = const_cast<Object*>(newer->m_next);
  Descriptor* owner = const_cast<Descriptor*>(newer->m_owner);
  // upgrade error if older == obj and owner == ME
  if ((older == obj) && (owner == this))
    return false;

  // just to be sure: we should not be the owner.
  assert(owner != this);

  // OK, we're destined to abort because someone else opened this object.
  // Let's just return true and let some other piece of code worry about
  // finding and handling the abort.
  //
  // NB: if we were to throw here, we'd throw RollBack() through an assert()
  //     call, and I don't want to do that
  return true;
}

/**
 *  Create a thread's transactional context.  This must be called before doing
 *  any transactions.  Since this function creates the thread's private
 *  allocator, this must be called before doing any allocations of memory that
 *  will become visible to other transactions, too.
 *
 * @param cm_type - string indicating what CM to use.
 *
 * @param validation - string indicating vis-eager, vis-lazy, invis-eager,
 * or invis-lazy
 *
 * @param use_static_cm - if true, use a statically allocated contention
 * manager where all calls are inlined as much as possible.  If false, use a
 * dynamically allocated contention manager where every call is made via
 * virtual method dispatch
 */
void thr_init(std::string cm_type, std::string validation,
              bool use_static_cm);

/**
 *  Call at thread destruction time to clean up and release any global TM
 *  resources.
 */
void thr_shutdown(unsigned long i);

/**
 *  Fence for privatization.  When a transaction's commit makes a piece of
 *  shared data logically private, the transaction must call fence() before
 *  creating an un_ptr to the privatized data.
 */
void fence();
void acquire_fence();
void release_fence();

void retry();

/**
 * Moves an object into the "transactional" world. It's not necessarily
 * "shared" right away, but the passed pointer is logically NULL after the
 * call.
 *
 * Attempting to access the Object directly after this call is a semantic
 * error.
 *
 * We expect that normal usage of this function looks like:
 *
 *    SharedHandle* sh = create_shared_handle(new Object());
 *
 */
inline SharedHandle* create_shared_handle(Object* obj) {
  return Descriptor::CreateShared(obj);
}

/*** Call this function when you think that you're not in a transaction. */
inline bool not_in_transaction() {
  Descriptor* const tx(currentDescriptor.get());
  return ((tx == NULL) || !(tx->inTransaction()));
}

/**
 * Called to open a shared handle for private access. Can only be called
 * outside of a transaction. The returned pointer is suitable for both read and
 * write access.
 */
Object* open_privatized(SharedHandle* const sh);

/**
 * Call to delete a shared handle that has been privatized in order to delete
 * it outside of a transaction.
 */
inline void delete_privatized(SharedHandle* sh) {
  currentDescriptor->mm.add(sh);
  currentDescriptor->mm.add(open_privatized(sh));
}

/**
 * When using some forms of privatization it is necessary to perform some
 * checks each time a private object is used.
 */
inline void on_private_use(SharedHandle* const sh, const Object* const obj) {
#ifdef STM_PRIV_NONBLOCKING
  if (sh->m_payload == obj)         // Testing for equality by address
    return;

  // [mfs] if asserts are off, is this correct?
  assert(obj == open_privatized(sh));
#endif
}

inline bool try_inevitable() { return currentDescriptor->try_inevitable(); }

} // namespace stm

#endif // RSTM_HPP
