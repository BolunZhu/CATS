///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007, 2008, 2009
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
// Lazy/Lazy with Priority Based CM (Fair)
//=============================================================================
//   As far as locking, orec-based STMs go, this is _supposed_ to be the least
//   prone to livelock possible (without sorting orecs before locking), and is
//   supposed to offer superior fairness via a contention manager that provides
//   true priority-based conflict resolution.  It achieves these goals by having
//   scalable time bases, a fixed lazy/lazy policy, code paths that avoid
//   aborting on conflicts that can be resolved through other means, and a
//   custom CM that makes reads and writes visible for prioritized transactions.
//=============================================================================

#ifndef FAIR_HPP__
#define FAIR_HPP__

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"
#include "cm/faircm.hpp"
#include "support/Privatization.hpp"

namespace stm
{
/*** Everything about this STM is encapsulated in its Descriptor */
class FairThread : public OrecWBTxThread {

  /*** STATIC FIELD DEFINITIONS ***/

  // choose a contention manager based on #defines
#if defined(STM_CM_LW)
  typedef BaseCM CMPolicy;
#elif defined(STM_CM_BLOOMPRIO)
  typedef BloomCM CMPolicy;
#elif defined(STM_CM_VISREADPRIO)
  typedef VisReadCM CMPolicy;
#endif

  /*** The global timestamp */
  static volatile unsigned long timestamp;

  /*** PER-INSTANCE FIELD DEFINITIONS ***/

  /*** contention manager */
  CMPolicy cm;

  /*** for caching the counter start time */
  unsigned start_time;

  /*** for caching the counter end time */
  unsigned end_time;

  /*** all of the lists of metadata that must be tracked */
  RedoLog     writes;
  OrecList    reads;
  OrecList    locks;
  WBMMPolicy  allocator;

  /*** for subsumption nesting */
  unsigned long nesting_depth;

  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;

  /*** privatization support */
  PrivPolicy priv;

  /*** for tracking statistics */
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  /*** my lock word */
  owner_version_t my_lock_word;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  FairThread(std::string cm_type);

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~FairThread() { }

  /*** inevitable read instrumentation */
  bool inev_read(void* addr) {
    BaseCM::InevStates state = cm.get_inev_state();
    // non-inevitable case
    if (state == BaseCM::REGULAR)
      return false;

    // mechanisms that don't require instrumentation
    else if (state == BaseCM::ISOLATED)
      return true;

    // instrumentation required

    // read the orec
    volatile orec_t* o = get_orec(addr);
    owner_version_t ovt;
    ovt.all = o->v.all;

    // if I've got a write lock, return immediately
    if (ovt.all == my_lock_word.all)
      return true;

    // get priority read
    //
    // NB: for bloom, llt inev uses the orec.  here, we use the address
    cm.beforeOpen((void*)addr);
    while (o->v.version.lock)
      spin64();
    return true;
  }

  /*** inevitable write instrumentation */
  bool inev_write(void* addr) {
    BaseCM::InevStates state = cm.get_inev_state();
    // non-inevitable case
    if (state == BaseCM::REGULAR)
      return false;

    // mechanisms that don't require instrumentation
    else if (state == BaseCM::ISOLATED)
      return true;

    // instrumentation required
    // need to get a write lock on this location

    // first get the orec
    volatile orec_t* o = get_orec(addr);
    owner_version_t ovt;
    ovt.all = o->v.all;

    // if I already have the lock, return
    if (ovt.all == my_lock_word.all)
      return true;

    // need to cas to acquire, retry until success
    while (true) {
      ovt.all = o->v.all;
      // wait for lock to be free
      if (ovt.version.lock) {
        spin64();
        continue;
      }
      if (bool_cas(&o->v.all, ovt.all, my_lock_word.all)) {
        o->p.all = ovt.all;
        // log the acquire
        locks.insert(const_cast<orec_t*>(o)); // cast away volatile
        return true;
      }
    }
  }

  /**
   * validate the read set by making sure that all orecs that we've read have
   * timestamps older than our start time, unless we locked those orecs.
   */
  void validate() {
    int myprio = cm.prio();
    // dispatch based on whether we have priority or not
    if (myprio)
      validate_prio(myprio);
    else
      validate_noprio();
  }

  /*** validate when transaction has nonzero priority but may hold locks */
  void validate_prio(int myprio) {
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if unlocked and newer than start time, abort
      if (!ovt.version.lock && (ovt.version.num > start_time))
        abort();

      // if locked and not by me, do a priority test
      else if (ovt.version.lock && (ovt.all != my_lock_word.all)) {
        // priority test... if I have priority, and the last unlocked
        // orec was the one I read, and the current owner has less
        // priority than me, wait
        if ((*i)->p.version.num <= start_time) {
          ovt.version.lock = 0;
          if (((FairThread*)ovt.owner)->cm.prio() < myprio) {
            spin128();
            continue;
          }
        }
        abort();
      }
    }
  }

  /*** validation for zero priority transactions that may hold locks */
  void validate_noprio() {
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if unlocked and newer than start time, abort
      if (!ovt.version.lock && (ovt.version.num > start_time))
        abort();

      // if locked and not by me, abort
      else if (ovt.version.lock && (ovt.all != my_lock_word.all)) {
        abort();
      }
    }
  }

  /**
   * fast-path validation without test if I hold lock.  use to validate when no
   * locks are held
   */
  __attribute__((flatten))
  void validate_fast() {
    // dispatch based on this txn's priority
    int myprio = cm.prio();

    if (myprio)
      validate_fast_prio(myprio);
    else
      validate_fast_noprio();
  }

  /**
   * fast-path validation without test if I hold lock.  use to validate when no
   * locks are held but transaction has priority
   */
  void validate_fast_prio(int myprio) {
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if unlocked and newer than start time, abort
      if (!ovt.version.lock && (ovt.version.num > start_time))
        abort();

      // if locked, abort unless priority says to wait
      else if (ovt.version.lock) {
        // priority test... if I have priority, and the last unlocked
        // orec was the one I read, and the current owner has less
        // priority than me, wait
        if ((*i)->p.version.num <= start_time) {
          ovt.version.lock = 0;
          if (((FairThread*)ovt.owner)->cm.prio() < myprio) {
            spin128();
            continue;
          }
        }
        abort();
      }
    }
  }

  /**
   * fast-path validation without test if I hold lock.  use to validate when no
   * locks are held and I have no priority
   */
  void validate_fast_noprio() {
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if unlocked and newer than start time, abort
      if (!ovt.version.lock && (ovt.version.num > start_time))
        abort();

      // if locked, abort unless priority says to wait
      else if (ovt.version.lock) {
        abort();
      }
    }
  }

  /*** lock write set: mechanism depends on if this tx has priority */
  void acquireLocks() {
    int myprio = cm.prio();
    if (myprio)
      acquireLocks_prio(myprio);
    else
      acquireLocks_noprio();
  }

  /*** lock all locations (priority version) */
  void acquireLocks_prio(int myprio) {
    // try to lock every location in the write set, loop increment is empty
    // because we want to "continue" the loop without incrementing i.
    for (RedoLog::iterator i = writes.begin(), e = writes.end(); i != e;) {
      // get orec, read its version#
      volatile orec_t* o = get_orec((void*)i->addr);
      owner_version_t ovt;
      ovt.all = o->v.all;

      // if orec not locked, lock it
      //
      // NB: if orec.version.num > start time, we may introduce inconsistent
      // reads.  Since most writes are also reads, we'll just abort under this
      // condition
      if (!ovt.version.lock) {
        if (ovt.version.num > start_time)
          abort();
        // may need CM here
        if (!bool_cas(&o->v.all, ovt.all, my_lock_word.all)) {
          spin128();
          continue;
        }
        // save old version to o->p
        o->p.all = ovt.all;
        // remember that we hold this lock
        locks.insert((orec_t*)o);
      }
      // else if we don't hold the lock abort
      else if (ovt.all != my_lock_word.all) {
        // priority test... if I have priority, and the last unlocked
        // version of the orec was the one I read, and the current
        // owner has less priority than me, wait
        if (o->p.version.num <= start_time) {
          ovt.version.lock = 0;
          if (((FairThread*)ovt.owner)->cm.prio() < myprio) {
            spin128();
            continue;
          }
        }
        abort();
      }
      ++i;
    }
  }

  /*** lock all locations */
  void acquireLocks_noprio() {
    // try to lock every location in the write set, loop increment is empty
    // because we want to "continue" the loop without incrementing i.
    for (RedoLog::iterator i = writes.begin(), e = writes.end(); i != e;) {
      // get orec, read its version#
      volatile orec_t* o = get_orec((void*)i->addr);
      owner_version_t ovt;
      ovt.all = o->v.all;

      // if orec not locked, lock it
      //
      // NB: if orec.version.num > start time, we may introduce inconsistent
      // reads.  Since most writes are also reads, we'll just abort under this
      // condition
      if (!ovt.version.lock) {
        if (ovt.version.num > start_time)
          abort();
        // may need CM here
        if (!bool_cas(&o->v.all, ovt.all, my_lock_word.all)) {
          spin128();
          continue;
        }
        // save old version to o->p
        o->p.all = ovt.all;
        // remember that we hold this lock
        locks.insert((orec_t*)o);
      }
      // else if we don't hold the lock abort
      else if (ovt.all != my_lock_word.all) {
        abort();
      }
      ++i;
    }
  }

  /*** run the redo log to commit a transaction */
  void redo() {
    for (RedoLog::iterator i = writes.begin(), e = writes.end(); i != e; ++i)
      *i->addr = i->val;
  }

  /*** release locks and restore the old version number */
  void releaseAndRevertLocks() {
    for (OrecList::iterator i = locks.begin(), e = locks.end(); i != e; ++i)
      (*i)->v.all = (*i)->p.all;
  }

  /*** release locks and update the version number at each position. */
  void releaseAndIncrementLocks() {
    // compute new version number based on end time
    owner_version_t newver;
    newver.all = 0;
    newver.version.num = end_time;

    for (OrecList::iterator i = locks.begin(), e = locks.end(); i != e; ++i)
      (*i)->v.all = newver.all;
  }

  /*** commit memops and release inevitability */
  void cleanup_inevitable() {
    // if I have locks, get a commit time
    if (locks.size() != 0)
      end_time = 1 + fai(&timestamp);

    // unmark inevitable reads
    cm.onCommit();
    cm.postCommit(locks);

    // release orecs and clean up
    releaseAndIncrementLocks();
    allocator.onTxCommit();
    common_cleanup();
  }

  /**
   * abort the current transaction by setting state to aborted, undoing all
   * operations, and throwing an exception
   */
  void abort() {
    assert(cm.get_inev_state() == BaseCM::REGULAR);

    tx_state = ABORTED;
    cleanup();
    cm.onAbort();
    num_aborts++;

    // unwind stack
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** reset lists and exit epochs */
  void common_cleanup() {
    reads.reset();
    writes.reset();
    locks.reset();
    priv.onEndTx();
  }

  /*** undo any externally visible effects of an aborted transaction */
  void cleanup() {
    // make sure we're aborted
    assert(tx_state == ABORTED);

    // release the locks and restore version numbers
    releaseAndRevertLocks();

    // undo memory operations
    allocator.onTxAbort();

    // reset all lists
    common_cleanup();
  }

  /*** transactional fence for privatization */
  void priv_fence() {
    priv.Fence();
  }

  /*** log transactional allocations so we can roll them back on abort */
  void* alloc_internal(size_t size) {
    return allocator.txAlloc(size);
  }

  /*** defer transactional frees until commit time */
  void txfree_internal(void* ptr) {
    allocator.txFree(ptr);
  }

  /**
   *  For Bloom inevitability, this implementation is far from optimal.
   *  The optimial implementation would combine all inserts into the read
   *  filter, do a single WBR, and then check all orecs.
   */
  __attribute__((flatten))
  void inevitable_read_prefetch(const void* addr, unsigned bytes) {
    // round the range up to nearest 4-byte boundary
    unsigned long* a = static_cast<unsigned long*>(const_cast<void*>(addr));
    unsigned b = bytes;
    if (b % 4 != 0)
      b += 4-(b%4);
    // call inev_read on every word in normalized address range
    for (unsigned i = 0; i < b/4; i+=4)
      inev_read(a+i);
  }

  /**
   *  For Bloom inevitability, this implementation is far from optimal.
   *  The optimial implementation would combine all inserts into the read
   *  filter, do a single WBR, and then check all orecs.
   */
  __attribute__((flatten))
  void inevitable_write_prefetch(const void* addr, unsigned bytes) {
    // round the range up to nearest 4-byte boundary
    unsigned long* a = static_cast<unsigned long*>(const_cast<void*>(addr));
    unsigned b = bytes;
    if (b % 4 != 0)
      b += 4-(b%4);
    // call inev_write on every word in normalized address range
    for (unsigned i = 0; i < b/4; i+=4)
      inev_write(a+i);
  }

  /*** try to become inevitable (must call before any reads/writes) */
  __attribute__((flatten))
  bool try_inev() {
    // multiple calls by an inevitable transaction have no overhead
    if (cm.get_inev_state() != BaseCM::REGULAR)
      return true;

    // currently, we only support becoming inevitable before performing
    // any transactional reads or writes
    assert((reads.size() == 0) && (writes.size() == 0));

    return cm.try_inevitable();
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<FairThread> Self;

  /*** PUBLIC METHODS:: Thread Management ***/

  static void init(std::string, std::string, bool);
  void dumpstats(unsigned long i);

  /*** PUBLIC METHODS:: Transaction Boundaries ***/

  /*** Begin a transaction */
  __attribute__((flatten))
  void beginTransaction(jmp_buf* buf) {
    assert(nesting_depth >= 0);
    if (nesting_depth++ != 0)
      return;

    // become active
    tx_state = ACTIVE;
    setjmp_buf = buf;
    allocator.onTxBegin();

    priv.onBeginTx();

    start_time = timestamp;
    end_time = 0; // so that we know if we need to get a timestamp in
    // order to safely call free()
    // notify CM
    cm.onBegin();
  }

  /*** commit a transaction */
  __attribute__((flatten))
  void commit() {
    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth-- > 1)
      return;

    // if inevitable, unwind inevitability and we're done
    if (cm.get_inev_state() != BaseCM::REGULAR) {
      num_commits++;
      tx_state = COMMITTED;
      cleanup_inevitable();
      return;
    }

    // if I don't have writes, I'm committed
    if (writes.size() == 0) {
      num_commits++;
      tx_state = COMMITTED;
      cm.onCommit();
      allocator.onTxCommit();
      common_cleanup();
      return;
    }

    // acquire locks
    acquireLocks();

    // fail if our writes conflict with a higher priority txn's reads
    if (!cm.preCommit(writes))
      abort();

    // increment the global timestamp if we have writes
    end_time = 1 + fai(&timestamp);

    // skip validation if nobody else committed
    if (end_time != (start_time + 1))
      validate();

    // set state to committed
    tx_state = COMMITTED;

    // run the redo log
    redo();

    // notify CM
    cm.onCommit();
    cm.postCommit(locks);

    // release locks
    releaseAndIncrementLocks();

    // remember that this was a commit
    num_commits++;

    // commit all frees, reset all lists
    allocator.onTxCommit();
    common_cleanup();
  }

  /**
   * Very simple for now... retry just unwinds the transaction, sleeps, and
   * then unwinds the stack
   */
  __attribute__((flatten))
  void retry() {
    // we cannot be inevitable!
    assert(cm.get_inev_state() == BaseCM::REGULAR);

    tx_state = ABORTED;
    cm.preRetry(reads);
    // note: if validate_fast fails, then we'll end up calling
    // cm.onAbort.
    validate_fast_noprio();

    // unroll mm ops
    allocator.onTxAbort();

    // reset all lists
    common_cleanup();

    cm.postRetry();
    num_retrys++;

    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** restart: just like retry, but we don't do any posting */
  void restart() {
    assert(cm.get_inev_state() == BaseCM::REGULAR);

    tx_state = ABORTED;

    // unroll mm ops
    allocator.onTxAbort();

    // reset all lists
    common_cleanup();

    cm.onAbort();
    num_restarts++;

    // unwind stack
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read */
  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    return addr_dispatch<FairThread, T, sizeof(T)>::read(addr, this);
  }

  /*** instrumented write */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    addr_dispatch<FairThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** instrumented read */
  __attribute__((flatten))
  void* stm_read_word(void** const addr) {
    // if inevitable, read inevitably and return
    if (inev_read((void*)addr))
      return *addr;

    // CM instrumentation
    cm.beforeOpen((void*)addr);

    // write set lookup
    if (writes.size() != 0) {
      wlog_t* const found = writes.find((void*)addr);
      if (found != NULL)
        return found->val;
    }

    // get the orec addr (cast away const)
    volatile orec_t* o = get_orec((void*)addr);

    while (true) {
      // read the orec BEFORE we read anything else
      owner_version_t ovt;
      ovt.all = o->v.all;
      CFENCE;

      // this tx doesn't hold any locks, so if the lock for this addr
      // is held, there is contention
      if (ovt.version.lock) {
        // just continue if owner is not active:
        ovt.version.lock = 0;
        if (((FairThread*)ovt.owner)->tx_state != ACTIVE)
          continue;
        while (o->v.all == ovt.all)
          yield_cpu();
        continue;
      }
      // if this location is too new, scale forward
      else if (ovt.version.num > start_time) {
        unsigned newts = timestamp;
        validate_fast();
        start_time = newts;
        continue;
      }

      // orec is unlocked, with ts <= start_time.  read the location
      void* tmp = *addr;

      // postvalidate AFTER reading addr:
      CFENCE;
      if (o->v.all != ovt.all)
        continue;

      // postvalidation succeeded.  log orec and return
      reads.insert((orec_t*)o);
      return tmp;
    }
  }

  /*** transactional write */
  __attribute__((flatten))
  void stm_write_word(void** addr, void* val) {
    // if inevitable, write inevitably and return
    if (inev_write((void*)addr)) {
      *addr = val;
      return;
    }

    // CM instrumentation
    cm.beforeOpen((void*)addr);

    // ensure that the orec isn't newer than we are... if so, validate
    volatile orec_t* o = get_orec((void*)addr);
    while (true) {
      // read the orec version number
      owner_version_t ovt;
      ovt.all = o->v.all;
      // if locked, spin and continue
      if (!ovt.version.lock) {
        // do we need to scale the start time?
        if (ovt.version.num > start_time) {
          unsigned newts = timestamp;
          validate_fast();
          start_time = newts;
          continue;
        }
        break;
      }
      spin128();
    }

    // Record the new value in a redo log
    writes.insert(wlog_t(addr, val));
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  static void fence() { Self->priv_fence(); }
  static void acquire_fence() { Self->priv_fence(); }
  static void release_fence() { Self->priv_fence(); }

  static void halt_retry() { Self->cm.haltRetry(); }

  static void setPrio(int i) { Self->cm.getPrio(i); }

  static bool try_inevitable() { return Self->try_inev(); }
  static void inev_write_prefetch(const void* addr, unsigned bytes)
  {
    Self->inevitable_write_prefetch(addr, bytes);
  }
  static void inev_read_prefetch(const void* addr, unsigned bytes)
  {
    Self->inevitable_read_prefetch(addr, bytes);
  }

}; // class stm::FairThread

} // namespace stm

#endif // FAIR_HPP
