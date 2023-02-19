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

#ifndef REDO_LOCK_HPP
#define REDO_LOCK_HPP

#include <string>
#include "support/defs.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/MiniVector.hpp"
#include "support/ConflictDetector.hpp"
#include "support/MMPolicy.hpp"
#include "support/Inevitability.hpp"
#include "cm/ContentionManager.hpp"
#include "cm/CMPolicies.hpp"
#include "support/atomic_ops.h"

#if defined(STM_ROLLBACK_SETJMP)
#include <setjmp.h>
#endif

namespace redo_lock
{
class Object;
class Descriptor;
class Validator;

/** Each thread needs a thread-local pointer to its Descriptor */
extern stm::ThreadLocalPointer<Descriptor> currentDescriptor;

/**
 *  If we're using nonblocking privatization, we need a flag to trigger
 *  immediate validation in active transactions whenever a privatizer commits.
 */
#ifdef STM_PRIV_NONBLOCKING
extern volatile unsigned long privatizer_clock;
#endif

/**
 * The entire metadata packet for an Object consists of a version number, an
 * owner pointer, and a redo log.
 *
 * Super-union represents version number, owner pointer, and lock.  The object
 * is locked if the verison is 2, the object is available if the version is
 * odd, and the object is owned (ver# = descriptor*) if the version is even.
 * Of course, 2 is even but 2 will never be a valid pointer.  Also note that if
 * /this/ is serving as a redo log, then the version is the old version, not
 * the version to be set upon successful redo.
 *
 * Then, in order to ensure that we access the entire metadata packet of an
 * object in a single atomic operation, we'll union the metadata packet with an
 * unsigned long long.
 *
 * This is actually pretty simple, but it's complicated by the fact that the
 * layout depends on the endianess of the architecture that we're on. Our calls
 * to casX() assume that the version is in the low word, and redoLog is in the
 * high word.
 *
 * the following code assumes big endian unless X86
 *
 * Since a pointer to an owner is always even, we can overload the version# of
 * a non-acquired object with the owner pointer of an acquired object.
 *
 * The following diagram is supposed to describe the structure of the
 * dataword. It can be viewd as a single unsigned long long, or a Object* and
 * an unsigned long or Descriptor*. The non-x86 version has the
 * Object*-unsigned long/Descriptor* words reversed.
 *
 *   +-----------------------------+------------------------------+
 *   |                unsigned long long (dword)                  |
 *   +-----------------------------+------------------------------+
 *
 *   - or -
 *
 *   +-----------------------------+------------------------------+
 *   |       Object* (log)         | unsigned long (version/lock) |
 *   +-----------------------------+------------------------------+
 *
 *   - or -
 *
 *   +-----------------------------+------------------------------+
 *   |       Object* (log)         |     Descriptor* (owner)      |
 *   +-----------------------------+------------------------------+
 *
 */
union metadata_dword_t {
  volatile unsigned long long dword;

  struct {
#if defined(__i386__) || defined(_MSC_VER) /* little endian */
    Object* volatile redoLog;
    union {
      volatile unsigned long version;
      Descriptor* volatile owner;
    } ver;
#else /* big endian */
    union {
      volatile unsigned long version;
      Descriptor* volatile owner;
    } ver;
    Object* volatile redoLog;
#endif
  } fields;

  /*** default constructor creates an unowned object at version 1 */
  metadata_dword_t() {
    fields.ver.version = 1;
    fields.redoLog = NULL;
  }

  /*** copy constructor */
  metadata_dword_t(const metadata_dword_t& w) { mvx(&w.dword, &dword); }

  /*** construct from a long long */
  metadata_dword_t(const unsigned long long w) { mvx(&w, &dword); }

  /**
   * not all architectures provide atomic 64-bit store, so overload
   * operator= to ensure that this is always written atomically
   */
  metadata_dword_t& operator=(const metadata_dword_t& w)
  {
    mvx(&w.dword, &dword); return *this;
  }
} __attribute__((aligned(8)));

/**
 * <code>Object</code> is a logically opaque class that all transactional
 * objects will inherit from. It's exported in the API for client use.
 *
 * In the redo_lock framework, <code>Object</code> adds the necessary metadata
 * for transactional objects. We don't want to let the client code mess with
 * this metadata, so the class is essentially private. On the other hand, the
 * library needs to manipulate the metadata, so there are lots of friends
 * declared.
 *
 * Object inherits from CustomAllocedBase so that it gets access to whatever
 * custom allocation strategy is currently in place.
 *
 * Transactional object have deferred delete semantics (sort of like garbage
 * collection). Because of this you can't really predict exactly when (or even
 * if) an Object's destructor is going to be called. Because of this we
 * recommend that your transactional objects have empty destructors.
 */
class Object : public stm::mm::CustomAllocedBase {
  friend class Validator;
  friend class Descriptor;

  /** The meta-data packet that makes /this/ shareable. */
  metadata_dword_t m_metadata;
};

/**
 * The primary class that implements the redo_lock algorithm. Contains
 * functionality for opening object, and tracking read and write sets, as well
 * as the transactional event methods. Also contains things like contention
 * managers and timing statistics management that are thread-local.
 */
class Descriptor {
  /**
   * Reads are tracked at the object level. A read log contains the object that
   * was read, and the version of the object. A log entry whose version no
   * longer matches the current version of the object has been written.
   */
  struct ReadLogEntry {
    Object*       shared;      // pointer to the object
    unsigned long version;     // expected version of the object

    ReadLogEntry(Object* sh = NULL, unsigned long v = 0)
      : shared(sh), version(v)
    { }
  };

  /**
   *  Type for entries in the eager write set.  We need to clone the "shared"
   *  version, so we need its size, and then we need to keep both around so we
   *  can free the right one at commit time and run redo().
   */
  struct EagerWriteLogEntry {
    Object* shared;    // pointer to the master version
    Object* clone;     // pointer to a buffer for uncommitted updates
    size_t  obj_size;  // object size, so we can clone via memcopy

    EagerWriteLogEntry(Object* sh = NULL, Object* cl = NULL,
                       size_t s = 0)
      : shared(sh), clone(cl), obj_size(s)
    { }
  };

  /**
   * Type for entries in the lazy write set.  Combines the requirements of an
   * EagerWriteLogEntry (managing clones) with the requirements of a
   * ReadLogEntry (validation)
   */
  struct LazyWriteLogEntry {
    Object*       shared;              // pointer to the master version
    unsigned long version;             // for validation
    Object*       clone;               // pointer to a buffer for updates
    size_t        obj_size;            // object size, for memcopy()

    LazyWriteLogEntry(Object* sh = NULL, unsigned long ver = 0,
                      Object* cl = NULL, size_t s = 0)
      : shared(sh), version(ver), clone(cl), obj_size(s) { }
  };

  typedef stm::MiniVector<ReadLogEntry> ReadLog;
  typedef stm::MiniVector<EagerWriteLogEntry> EagerWriteLog;
  typedef stm::MiniVector<LazyWriteLogEntry> LazyWriteLog;

 public:
  /*** COMMITTED, ABORTED, or ACTIVE */
  volatile unsigned long tx_state;

#ifdef STM_PRIV_NONBLOCKING
  /** If we're using nonblocking privatization, we need to cache the
      privatizer clock locally. */
  unsigned long privatizer_clock_cache;
#endif

  /** Wrapper for a contention manager. */
  stm::HybridCMPolicy cm;

  /** Eager/lazy acquisition strategy flag. */
  const bool isLazy;

  /** Interface to thread-local allocator that manages reclamation on
      abort/commit automatically. */
  stm::RADDMMPolicy mm;

  /** Inevitability support */
  stm::InevPolicy inev;
  bool try_inevitable() {
    // handle duplicate calls before doing assertion below
    if (inev.isInevitable())
      return true;
    // currently, we only support becoming inevitable before performing any
    // transactional reads or writes
    assert((reads.size() == 0) && (eagerWrites.size() == 0) &&
           lazyWrites.size() == 0);
    return inev.try_inevitable();
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
  /*** call this from catch */
  __attribute__((flatten))
  void onError() {
    if (nesting_depth-- > 1)
      return;
    // assert not inevitable?
    tx_state = stm::ABORTED;
    cleanup();
    num_aborts++;
  }
#endif

  /**
   * In an object-based redo-lock system, updates to shared objects are
   * acutally done on thread local clones of the object. Once a transaction has
   * committed, all of these local writes have to be copied back into the
   * actual shared object ("redone"). This is the method for performing the
   * copy-back (copy from /clone/ to /this/).
   *
   * @param exp_owner   The Descriptor* that we expect to be the owner of
   *                    /this/ object.
   * @param clone       A pointer to the cloned information that we are
   *                    copying back.
   * @param obj_size    The size of the object. We're dealing with classes
   *                    that inherit from Object*, so there isn't any good
   *                    way to know this automatically without making
   *                    things super-complicated clone-wise.
   */
  static void redoClone(Object* obj, const Descriptor* const exp_owner,
                        const Object* const redo_log, size_t objsize) {
    // try to lock the object; if we can't lock, then we just bail
    if (casX(&obj->m_metadata.dword,
             reinterpret_cast<unsigned long>(exp_owner),
             reinterpret_cast<unsigned long>(redo_log), 2,
             reinterpret_cast<unsigned long>(redo_log))) {
      // we locked the object; now apply the redo log

      // source and destination start right after metadata packet bits to copy
      // is objsize - size of metadata packet, where metadata packet size
      // includes vtable pointer
      memcpy(&obj->m_metadata + 1, &redo_log->m_metadata + 1,
             objsize - sizeof(Object));

      // instead of CASing to release the lock, let's just do an atomic 2-word
      // store
      metadata_dword_t replace(redo_log->m_metadata);
      replace.fields.ver.version += 2;
      replace.fields.redoLog = 0;
      obj->m_metadata = replace;
    }
  }

  /**
   * In an object-base redo-lock system, transactional writes to shared objects
   * are actually performed on thread local clones of the object. If a
   * transaction aborts, we need to delete these clones, as well as possibly
   * fix the meta-data of the actual object (/this/ object) to indicate that
   * our clone is gone.
   */
  static void clearClone(Object* obj, const Descriptor* const exp_owner,
                         const Object* const exp_log) {
    casX(&obj->m_metadata.dword,
         reinterpret_cast<unsigned long>(exp_owner),
         reinterpret_cast<unsigned long>(exp_log),
         exp_log->m_metadata.fields.ver.version, 0);
  }

  /** If we're using conflict detection heuristic to avoid validation,
      this stores our local heuristic information. */
  stm::ValidationPolicy conflicts;

  // logs
  ReadLog reads;
  EagerWriteLog eagerWrites;
  LazyWriteLog lazyWrites;

  /**
   * During lazy acquisition we keep track of how many of our write objects we
   * successfully acquired, so that if we abort at some point we can limit the
   * number we need to release.
   */
  unsigned long acq_count;

  /** The current depth of nested transactions, where 0 is no transaction. */
  unsigned int nesting_depth;

#if defined(STM_ROLLBACK_SETJMP)
  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;
#endif

  /**
   * The transaction descriptor is a thread-local object for all of our current
   * library implementations. This constructor is probably only called during
   * /stm_init()/. Allows a dynamically configurable contention management
   * scheme. Also allows dynamic selection of lazy or eager writes.
   *
   * @param dynamic_cm      Passed along to the contention manager (cm)
   * @param val             The command line parameter for the
   *                        read-write scheme we're using. Should be
   *                        "invis-eager" or "invis-lazy."
   * @param use_static_cm   A flag indicating if we want to be able to
   *                        dynamically change the contention management
   *                        scheme after descriptor construction.
   */
  Descriptor(std::string dynamic_cm, std::string val, bool use_static_cm)
    : tx_state(stm::COMMITTED),       // the state is COMMITTED in tx #0
#ifdef STM_PRIV_NONBLOCKING
      // cache the privatizer commit count if we need to
      privatizer_clock_cache(privatizer_clock),
#endif
      cm(use_static_cm, dynamic_cm),  // set up contention management
      isLazy((val != "invis-eager") && (val != "vis-eager")),
      mm(),                           // set up deferred reclamation
      conflicts(),                    // heap based bookkeeping fields
      reads(64),
      eagerWrites((isLazy) ? 0 : 64),
      lazyWrites((isLazy) ? 64 : 0),
      nesting_depth(0),
      num_commits(0), num_aborts(0)
  { }

  /** Destructor is currently a nop. */
  ~Descriptor() { }

  /////////////////////////////////////////////////////////////////////////
  // Transactional control functions
  /////////////////////////////////////////////////////////////////////////

  /**
   * Your basic begin transaction method. Prepares all of the logs and the
   * transaction's state.
   */
#if defined(STM_ROLLBACK_SETJMP)
  void begin_transaction(jmp_buf* buf)
#else
    void begin_transaction()
#endif
  {
    // only for outermost transaction
    if (nesting_depth++ != 0)
      return;

    inev.onBeginTx();
    conflicts.onTxBegin();
    mm.onTxBegin();
    tx_state = stm::ACTIVE;           // mark myself active
#if defined(STM_ROLLBACK_SETJMP)
    setjmp_buf = buf;
#endif
    cm.onBeginTx();
  }

  /**
   * The call to commit a transaction. This will do all of the stuff required
   * to commit a transaction, like validation read-write sets, acquiring
   * write-sets in a lazy setting, and writing back all of the clones to their
   * in-place versions.
   *
   * A side effect of this method is that the tx_state is updated to commited
   * and the logs are cleared.
   *
   * Commit failure is indicated by throwing an stm::Aborted exception.
   *
   * @throws    stm::RollBack
   */
  void commit() {
    if (nesting_depth > 1) {
      nesting_depth--;
      return;
    }

    // only try to commit if we are still ACTIVE
    if (tx_state != stm::ACTIVE)
      abort();

    // Contention Manager notification
    cm.onTryCommitTx();

    if (inev.isInevitable()) {
      // inevitable transaction doesn't need to do anything to its read and
      // write sets, but it must release the inevitability token
      tx_state = stm::COMMITTED;
      cm.onTxCommitted();
      inev.onInevCommit();
    }
    else {
      // acquire objects that were open_RW'd lazily
      if (isLazy)
        acquireLazyWrites();

      // validate if necessary
      if (!conflicts.tryCommit()) {
        verifyReads();
        conflicts.forceCommit();
      }

      // cas status to commit; if cas fails then I've been aborted
      if (!bool_cas(&(tx_state), stm::ACTIVE, stm::COMMITTED))
        abort();

      cm.onTxCommitted();

      redoEagerWrites();
      redoLazyWrites();
      reads.reset();
    }

    // commit memory changes and reset memory logging
    mm.onTxEnd(stm::COMMITTED);
    inev.onEndTx();

    num_commits++;
    nesting_depth--;
  }

  /**
   * Called to abort a transaction. This sets the transaction's state, and will
   * throw an stm::Aborted exception unless you tell it not to. The option to
   * tell it not to is only used in one place, as part of the API
   * END_TRANSACTION macro.
   *
   * @throws                stm::RollBack
   */
  void abort() {
    // need to assert not inevitable
    tx_state = stm::ABORTED;
    cleanup();
    num_aborts++;
#if defined(STM_ROLLBACK_THROW)
    throw stm::RollBack();
#else
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
#endif
  }

  /**
   * Contains the functionality to rollback a failed transaction. It
   * essentially jsut goes through the logs and fixes up any metadata that
   * needs to be fixed.
   */
  void cleanup() {
    // we'd better be aborted if we're here
    assert(tx_state == stm::ABORTED);

    // notify CM
    cm.onTxAborted();

    // restore the headers of any objects that we acquired
    clearLazyWrites();
    clearEagerWrites();

    // null out invis read list
    reads.reset();

    // commit memory changes and reset memory logging
    mm.onTxEnd(tx_state);

    // exit inevitability
    inev.onEndTx();
  }

  /**
   * Just a performance optimization. Sometimes it makes sense to verify a log
   * when we are inserting a new entry. We combine this into one operation so
   * that we don't iterate through the log twice.
   *
   * As opposed to the simple insert function, this /does/ check to see if the
   * object has already been inserted. In the case where the object already
   * exists and is current, we don't add it again.
   *
   * If the verification fails we'll abort
   *
   * @param shared  The address of the object that we read from.
   * @param version The version of the object that we read.
   *
   * @throws        stm::RollBack
   */
  void insertAndVerifyReads(Object* shared, unsigned long version) {
    assert(shared);
    for (ReadLog::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      if (!isCurrent(i->shared, i->version))
        abort();
      if (shared == i->shared)
        return;
    }
    reads.insert(ReadLogEntry(shared, version));
  }

  /**
   * Removes an object from our read set. Used during early release
   * exclusively.
   *
   * @param shared  The address of the object that we're removing.
   */
  void removeRead(const Object* const shared) {
    for (ReadLog::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      if (shared == i->shared) {
        reads.remove(i);
        return;
      }
    }
  }

  /**
   * Runs through all of the read set entries making sure that the version
   * number hasn't changed. If a version number is no longer up to date then
   * abort and restart the transaction.
   *
   * @throws    stm::RollBack
   */
  void verifyReads() {
    for (ReadLog::iterator i = reads.begin(), e = reads.end(); i != e; ++i)
      if (!isCurrent(i->shared, i->version))
        abort();
  }

  /**
   * Runs through all of the log entries making sure that the version number
   * hasn't changed. If a version number is no longer up to date then abort and
   * restart the transaction
   *
   * @throws    stm::RollBack
   */
  void verifyLazyWrites() {
    for (LazyWriteLog::iterator i = lazyWrites.begin(),
           e = lazyWrites.end(); i != e; ++i)
      if (!isCurrent(i->shared, i->version))
        abort();
  }

  /**
   * When a transaction aborts, we need to go through and clear all of the
   * clones that we made. This means that we need to clean up the metadata of
   * any of the in-place objects that we have already acquired. We don't
   * actaully have to do anything with the memory allocated by the clones
   * because that is managed by the transaction descriptor's mm member.
   *
   * We optimize this operation by tracking the number of write-set objects
   * that have already been acquired in the acq_count member. This works
   * because we never remove an individual object from the write-set, so the
   * order of objects in the log doesn't change.
   */
  void clearLazyWrites() {
    // I'm cheating here because I know that the iterator type for a Minivector
    // is just a pointer. We only want to clear acquired objects.
     for (LazyWriteLog::iterator i = lazyWrites.begin(),
            e = lazyWrites.begin() + acq_count; i != e; ++i)
      clearClone(i->shared, this, i->clone);
    acq_count = 0;
    lazyWrites.reset();
  }

  /**
   * When a transaction commits, all of the transaction's writes need to be
   * written back into their in-place locations. This method iterates through
   * the log and coppies-back all of the clones.
   *
   * This method assumes that all of the write-set objects have already been
   * successfully acquired with the acquireAll method.
   */
  void redoLazyWrites() {
    for (LazyWriteLog::iterator i = lazyWrites.begin(),
           e = lazyWrites.end(); i != e; ++i)
      redoClone(i->shared, this, i->clone, i->obj_size);

    acq_count = 0;
    lazyWrites.reset();
  }

  /**
   * In a lazy acquire system, at commit time, we need to iterate through the
   * write log and acquire all of the objects that we wrote. If we fail this
   * process, we'll need to release all of the objects that we have acquired.
   *
   * This method iterates through the log and does the acquires. If we fail to
   * acquire one of the objects failure is indicated by throwing an
   * stm::Aborted exception.
   *
   * A side effect of this call is that the acq_count member is updated with
   * the number of objects that we successfully acquired. This is used by a
   * subsequent clearAll call as a performance optimization, so that we know
   * how much of the log we need to release.
   *
   * Successfully acquiring the whole log is indicated by /not/ throwing an
   * exception. This is a little bit odd an is a possible place to look at for
   * design changes.
   *
   * @throws    stm::RollBack
   */
  void acquireLazyWrites() {
     for (LazyWriteLog::iterator i = lazyWrites.begin(),
            e = lazyWrites.end(); i != e; ++i) {
      if (!acquire(i->shared, i->version, i->clone))
        abort();
      else
        ++acq_count;
    }
  }

  /**
   * With the lazy acquisition algorithm, we occassionally need to be able to
   * lookup an object in the write-set.
   *
   * @param shared  The object that we're looking for. This is the
   *                in-place address, not a clone's address.
   *
   * @returns       The address of the clone, if one exists. If the object
   *                isn't in the write-set we return NULL.
   */
  Object* findLazyWrite(const Object* const shared) const {
    assert(shared);
    for (LazyWriteLog::iterator i = lazyWrites.begin(),
           e = lazyWrites.end(); i != e; ++i)
      if (shared == i->shared)
        return i->clone;
    return NULL;
  }

  /**
   * Simple retry is just to abort which will trigger a rollback and restart.
   */
  void retry() {
    // we cannot be inevitable!
    assert(!inev.isInevitable());
    tx_state = stm::ABORTED;
    cleanup();
    num_retrys++;
    sleep_ms(1);

#if defined(STM_ROLLBACK_THROW)
    throw stm::RollBack();
#else
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
#endif
  }

  /////////////////////////////////////////////////////////////////////////
  // Transactional object management - methods used in the API to manage
  // shared data.
  /////////////////////////////////////////////////////////////////////////

  /**
   * Your basic open for reading call. Should be used when the code first
   * encounters a shared object. The function returns the address of the object
   * that should be read from. If the object hasn't ever been opened by the
   * current transaction, then the returned address will just be te same as the
   * /obj/ parameter.
   *
   * If the object has been opened for writing by the current transaction, then
   * the address of the clone will be returned, so that reads through the
   * pointer will see updates made by any aliased writable pointers.
   *
   * @param obj         The address of the in-place object we'd like to read.
   * @param v           A reference to a validator that the API will use
   *                    for future read validation, to make sure that
   *                    reads through the returned pointer come from a
   *                    consistent version of the object.
   * @param obj_size    The size, in bytes, of the derived object. This is
   *                    needed because we want to be able to be able to
   *                    clean-up the /obj/ object if a committed or
   *                    aborted transaction owns it.
   *
   * @returns           This will return the address of an object to do
   *                    all future reads through. It's probably equal to
   *                    the /obj/ parameter, but might be the address of a
   *                    clone if the transaction already opened /obj/ for
   *                    writing.
   */
  Object* openReadOnly(Object* obj, Validator& v, size_t obj_size);

  /**
   * The basic open for writing call. We're always going to look through our
   * write-set to see if we've already got a writable clone for /obj/.
   *
   * @param obj         The address of the in-place object that we'd like
   *                    to write transactionally.
   * @param obj_size    The size of the derived object, so that we can
   *                    correctly do the write-back if the transaction
   *                    commits.
   *
   * @returns           A clone suitable for making buffered writes to.
   */
  Object* openReadWrite(Object* obj, size_t obj_size);

  /**
   * The library call to open a privatized <code>Object</code>. Privated data
   * is modified in-place, so we don't actually return anything here. We're
   * also not going to delete or move the object, so it's passed by reference
   * for clarity and self-documentation purposes.
   */
  static void open_privatized(Object* obj, size_t objsize);

  /**
   * Will lookup the in-place version associated with the object. This allows
   * the API to let the client initialize multiple smart pointers with the same
   * <code>Object*</code> and get the expected behavior.
   *
   * @param obj  The object that we're trying to get an in-place version of
   *
   * @returns    The in-place version for the object, or NULL if obj is
   *             NULL.
   */
  static Object* get_inplace_version(Object* const obj) {
    if (!obj)
      return NULL;

    // get a snapshot of the header
    metadata_dword_t snap(obj->m_metadata);

    // if the redoLog is not null and the version is odd, then we're
    // looking at the redo log (and the redoLog field is actually
    // pointing to the inplace version)
    return ((snap.fields.redoLog != NULL) &&
            ((snap.fields.ver.version & 1) == 1))
      ? snap.fields.redoLog : obj;
  }

  /**
   * Advanced users will sometimes write algorithms in which they want to
   * "forget" about an object that they read in the past. Our benchmark example
   * is a linked list where we don't really care if someone writes the first
   * element in the list if we are just trying to read the 10th object.
   *
   * This method removes an object from our read set. It should only be used by
   * super-experts, as incorrect use can cause very-difficult-to- debug
   * problems.
   *
   * @param obj     The address of the in-place object that we'd like to
   *                remove from the read-set.
   */
  void release(const Object* const obj) {
    // NB: no need to check if I own this in my eager or lazy writeset; that's
    //     orthogonal to whether it's in my read set, just search for it and
    //     remove it.
    removeRead(obj);
  }

  /**
   * In a transaction, we need to defer deletes until the transaction
   * commits. Even worse, we need to defer deletes until we're sure that no
   * other thread is still looking at a to-be-deleted object in a doomed
   * transaction.
   *
   * This is the method that the API calls to delete a shared object.
   *
   * @param obj     The object to delete.
   */
  void deleteTransactionally(Object* obj) {
    mm.deleteOnCommit.insert(obj);
  }

  /////////////////////////////////////////////////////////////////////////
  // Transactional utilities - used by the API for debugging, possibly
  // passed through to client code for branching based on transactional
  // contexts.
  /////////////////////////////////////////////////////////////////////////

  /**
   * Simple method to tell you if you're currently inside of a transactional
   * context.
   */
  bool inTransaction() const { return (tx_state != stm::COMMITTED); }

  /**
   * A utility to tell you if an object is aliased. The first parameter is the
   * address of the in-place object that you're checking, and the second
   * pointer is the address that you think is the current address of the
   * object.
   *
   * If /shared/ is open for reading only, then /active/ should equal
   * /shared/. If /shared/ is open for writing, then /active/ should be the
   * current clone of /shared/.
   */
  bool isAliased(const Object* const obj, const Object* const active) const {
    if (obj == NULL)
      return false;
    else if (obj == active)
      return false;

    // read the header of /obj/ to a local
    metadata_dword_t snap(obj->m_metadata);

    // if the version is odd, then /this/ is unowned.  since it isn't
    // our t, we must have been aborted.  That's fine, just return
    // true.
    if ((snap.fields.ver.version & 1) == 1)
      return false;

    // if the version is 2, then /this/ is locked, which is fine too
    else if (snap.fields.ver.version == 2)
      return false;

    // otherwise we have a problem if /this/ is the owner and obj !=
    // obj->redo_log
    if ((snap.fields.ver.owner == this) &&
        (active != snap.fields.redoLog))
      return true;

    return false;
  }

  /**
   *  Used in various places for a thread to get access to it's thread local
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
   * When using non-blocking privatization, this is called to check the
   * privatization clock. This will make sure that a doomed transaction doesn't
   * do anything dangerous with an object that is possibly being written
   * non-transactionally. It will throw an stm::RollBack transaction if
   * validation fails.
   *
   * @throws    stm::RollBack
   */
  void checkPrivatizerClock() {
#ifdef STM_PRIV_NONBLOCKING
    while (privatizer_clock_cache != privatizer_clock) {
      privatizer_clock_cache = privatizer_clock;
      validate();
      verifySelf();
    }
#endif
  }

  /**
   * Called to see if an object's metadata is still current.
   *
   * @param obj     The object to check.
   * @param exp_ver The version that we expect the object to be at.
   *
   * @returns       true if the object is current.
   */
  bool isCurrent(const Object* const obj, unsigned long version) const {
    // check if shared.header and expected_version agree on the currently
    // valid version of the data
    assert(obj);
    metadata_dword_t snap(obj->m_metadata);

    // if nobody has acquired this object, then the version will match
    // expected_ver
    if (snap.fields.ver.version == version)
      return true;

    // uh-oh!  It doesn't match.  If the version number is odd or 2, then we
    // must immediately fail.  If the version number is not a pointer to
    // /this/, then we must fail.  If the version number is a pointer to
    // /this/, then we succeed only if the clone's version is version.  So to
    // keep it easy, let's compare the ver.owner to /this/...
    if (snap.fields.ver.owner != this)
      return false;

    // ok, it should be safe to dereference the redoLog
    return snap.fields.redoLog->m_metadata.fields.ver.version == version;
  }

  /**
   * Manipulates the metadata of an object to acquire it. Used in both eager
   * and lazy acquisition schemes, just at different times in the transaction.
   *
   * @param obj     The object to acquire.
   * @param exp_ver The version we think the object should be at.
   * @param clone   The writable clone of the object.
   *
   * @returns       true if we acquired the object successfully.
   */
  bool acquire(Object* obj, unsigned long exp_ver, Object* clone) {
    if (!casX(&obj->m_metadata.dword, exp_ver, 0,
              reinterpret_cast<unsigned long>(this),
              reinterpret_cast<unsigned long>(clone)))
      return false;

    // successfully acquired this Object
    cm.onOpenWrite();
    return true;
  }

 private:
  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;

 public:
  unsigned getCommits() { return num_commits; }
  unsigned getAborts()  { return num_aborts;  }
  unsigned getRetrys()  { return num_retrys;  }

 private:
  /**
   * Called internally to check and see if I've been aborted by some other
   * transaction. Indicates failure by throwing an stm::Aborted transaction.
   *
   * @throws    stm::RollBack
   */
  void verifySelf() {
    if (tx_state == stm::ABORTED)
      abort();
  }

  /**
   * Runs through the transaction's versioned logs (definately read log,
   * possibly lazy writes), checking to make sure that all object's versions
   * are still what we expect them to be. Indicates failure by throwing
   * stm::Aborted.
   *
   * @throws    stm::RollBack
   */
  void validate() {
    mm.onValidate();
    verifyReads();
    verifyLazyWrites();
  }

  /**
   * When the current transaction aborts we have to run through all of our
   * write-set and clear out all of the clones that we have made.
   *
   * A side effect of this operation is that the log itself is cleared, which
   * is a simple O(1) operation when using the default MiniVector<> log
   * implementation.
   */
  void clearEagerWrites() {
     for (EagerWriteLog::iterator i = eagerWrites.begin(),
            e = eagerWrites.end(); i != e; ++i)
      clearClone(i->shared, this, i->clone);
    eagerWrites.reset();
  }

  /**
   * In our object-based, redo-log systsem, a committing transaction needs to
   * go through and write-back all of the transactional updates that it made to
   * all of the clones. This does that.
   *
   * A side effect of this operation is that the log itself is cleared, which
   * is a simple O(1) operation when using the default MiniVector<> log
   * implementation.
   */
  void redoEagerWrites() {
    for (EagerWriteLog::iterator i = eagerWrites.begin(),
           e = eagerWrites.end(); i != e; ++i)
      redoClone(i->shared, this, i->clone, i->obj_size);
    eagerWrites.reset();
  }

 public:
  /**
   * Perform the 'transactional fence'.  If we're using a Validation fence or
   * Transactional fence, then globalEpoch.HeavyFence() will do all the work
   * (its behavior varies based on compile flags).  If we're doing
   * obstruction-free privatization, then simply increment the global
   * privatization counter.
   */
  void fence() {
#ifdef STM_PRIV_NONBLOCKING
    fai(&privatizer_clock);
#else
    mm.waitForDominatingEpoch();
#endif
  }
}; // class redo_lock::Descriptor

/**
 * For correctness this object-based, redo-log implementation needs to check to
 * make sure that multiple reads associated with the same object come from the
 * same version of the object. This is because someone can come along and
 * change the version of an object at any time, and we won't know about it
 * until we validate.
 *
 * The validator class is used in the API to cache a version number that can
 * then be validated against an object after every read to make sure that it
 * hasn't changed.
 */
class Validator {
  unsigned long m_version;

 public:
  void config(unsigned long v) { m_version = v; }

  void validate(Descriptor& tx, const Object& obj) const {
    if ((m_version) && (obj.m_metadata.fields.ver.version != m_version))
      tx.abort();
    tx.checkPrivatizerClock();
  }

}; // class redo_lock::Validator
} // namespace redo_lock

namespace stm {
/**
 * Create a thread's transactional context.  This must be called before doing
 * any transactions.  Since this function creates the thread's private
 * allocator, this must be called before doing any allocations of memory that
 * will become visible to other transactions, too.
 *
 * @param cm_type - string indicating what CM to use.
 *
 * @param validation - string indicating invis-eager or invis-lazy
 *
 * @param use_static_cm - if true, use a statically allocated contention
 * manager where all calls are inlined as much as possible.  If false, use
 * a dynamically allocated contention manager where every call is made via
 * virtual method dispatch
 */
inline void init(std::string cm_type, std::string validation,
                 bool use_static_cm) {
  // initialize mm for this thread
  mm::initialize();

  // create a Descriptor for the new thread and save it in thread-local
  // storage
  redo_lock::currentDescriptor.set(new redo_lock::Descriptor(cm_type,
                                                             validation,
                                                             use_static_cm));
}

/**
 *  Shut down a thread's transactional context.  currently just dumps stats
 */
void shutdown(unsigned long i);

/**
 *  All other functions in this namespace simply forward to the appropriate
 *  Descriptor method
 */

/*** get a chunk of garbage-collected, tx-safe memory */
inline void* tx_alloc(size_t s) {
  return redo_lock::currentDescriptor->mm.txAlloc(s);
}

/*** return memory acquired through tx_alloc */
inline void tx_free(void* ptr) {
  redo_lock::Descriptor* tx = redo_lock::currentDescriptor.get();
  if (tx->tx_state == stm::COMMITTED)
    tx->mm.txFree(ptr);
  else
    tx->mm.deleteOnCommit.insert((stm::mm::CustomAllocedBase*)ptr);
}

/**
 * The library call to delete a privatized <code>Object</code>.
 */
inline void delete_privatized(redo_lock::Object* obj) {
  redo_lock::currentDescriptor->mm.add(obj);
}

/*** privatization */
inline void fence() { redo_lock::currentDescriptor->fence(); }
inline void acquire_fence() { redo_lock::currentDescriptor->fence(); }
inline void release_fence() { redo_lock::currentDescriptor->fence(); }

/*** try to become inevitable */
inline bool try_inevitable() {
  return redo_lock::currentDescriptor->try_inevitable();
}

/*** inevitable read prefetching (nop since only GRL is supported) */
inline void inev_read_prefetch(const void* addr, unsigned bytes) { }

/*** inevitable write prefetching (nop since only GRL is supported) */
inline void inev_write_prefetch(const void* addr, unsigned bytes) { }

/*** transaction retry */
inline void retry() { redo_lock::currentDescriptor->retry(); }

} // namespace stm

#endif // REDO_LOCK_HPP
