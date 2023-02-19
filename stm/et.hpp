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
// Extendable Timestamp STM (ET)
//=============================================================================
//   Word-Based, Timestamp, Locking STM with extendable timestamps.  This
//   supports three strategies: encounter-time locking with direct update (ee),
//   encounter-time locking with deferred update (el), and commit-time locking
//   with deferred update (ll).  To choose between these options, use "ee",
//   "el", and "ll" as the second parameter to init()
//
//   NB: There are many ways to manage timestamps.  We take the approach that
//   (1) final validation should be avoided when possible, (2) it's OK to
//   forbid some valid WAW orderings in order to simplify lock
//   acquisition/validation and (3) skipping incarnation numbers in timestamps
//   is OK as long as ee aborts increment the orec timestamp.
//=============================================================================


#ifndef ET_HPP__
#define ET_HPP__

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "cm/WordBased.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"
#include "support/Inevitability.hpp"
#include "support/Privatization.hpp"

namespace stm {

/*** Everything about this STM is encapsulated in its Descriptor */
class ETThread : public OrecWBTxThread {
  /*** STATIC FIELD DEFINITIONS ***/

  /*** The global timestamp */
  static volatile unsigned long timestamp;

  /*** PER-INSTANCE FIELD DEFINITIONS */

  /*** for caching the counter start time */
  unsigned start_time;

  /*** for caching the counter end time */
  unsigned end_time;

  /*** all of the lists of metadata that must be tracked */
  RedoLog     redolog;
  ValueList   undolog;
  OrecList    reads;
  OrecList    locks;
  WBMMPolicy  allocator;

  /*** for subsumption nesting */
  unsigned long nesting_depth;

  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;

  /*** inevitability support */
  InevPolicy inev;

  /*** privatization support */
  PrivPolicy priv;

  /*** for tracking statistics */
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  /*** my lock word */
  owner_version_t my_lock_word;

  /*** contention management */
  WBCM cm;

  /**
   * support for EagerEager, EagerLazy, or LazyLazy conflict detection / memory
   * updating
   */
  enum Modes { EagerEager = 0, EagerLazy = 1, LazyLazy = 2 } mode;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  ETThread(std::string cm_type, std::string validation);

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~ETThread() { }

  /**
   * validate the read set by making sure that all orecs that we've read have
   * timestamps older than our start time, unless we locked those orecs.
   */
  void validate() {
    for (OrecList::iterator i = reads.begin(); i != reads.end(); ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if unlocked and newer than start time, abort
      if (!ovt.version.lock && (ovt.version.num > start_time))
        abort();
      // if locked and not by me, abort
      else if (ovt.version.lock && (ovt.all != my_lock_word.all))
        abort();
    }
  }

  /**
   * fast-path validation without test if I hold lock.  use to validate when no
   * locks are held (lazy acquire or read-only)
   */
  void validate_fast() {
    for (OrecList::iterator i = reads.begin(); i != reads.end(); ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // abort if locked, or if unlocked but the timestamp is newer
      // than my start time
      if (ovt.version.lock || (ovt.version.num > start_time))
        abort();
    }
  }

  /*** lock all locations */
  void acquireLocks() {
    // try to lock every location in the write set
    for (RedoLog::iterator i = redolog.begin(); i != redolog.end(); ++i) {
      // get orec, read its version#
      volatile orec_t* o = get_orec(i->addr);
      owner_version_t ovt;
      ovt.all = o->v.all;

      // if orec not locked, lock it
      //
      // NB: if orec.version.num > start time, we may introduce inconsistent
      // reads.  Since most writes are also reads, we'll just abort under this
      // condition.  This can introduce false conflicts
      if (!ovt.version.lock) {
        if (ovt.version.num > start_time)
          abort();
        // NB: if we can't lock the location, we could call CM instead of just
        //     aborting
        if (!bool_cas(&o->v.all, ovt.all, my_lock_word.all))
          abort();
        // save old version to o->p
        o->p.all = ovt.all;
        // remember that we hold this lock
        locks.insert((orec_t*)o);
      }
      // else if we don't hold the lock abort
      // NB: we could have a CM call in here before aborting
      else if (ovt.all != my_lock_word.all) {
        abort();
      }
    }
  }

  /*** run the undo log to roll back a transaction */
  void undo() {
    // undo log must go in reverse, since we might have duplicate entries and
    // the log is in chronological order
    ValueList::iterator i = undolog.end();
    while (--i >= undolog.begin())
      *i->addr = i->val;
  }

  /*** run the redo log to commit a transaction */
  void redo() {
    for (RedoLog::iterator i = redolog.begin(); i != redolog.end(); ++i)
      *i->addr = i->val;
  }

  /*** on abort, release locks and restore the old version number.  Don't do
       this if you are using undo logs */
  void releaseAndRevertLocks() {
    for (OrecList::iterator i = locks.begin(); i != locks.end(); ++i)
      (*i)->v.all = (*i)->p.all;
  }

  /*** on commit, release locks and update the version numbers */
  void releaseAndUpdateLocks() {
    // compute new version number based on end time
    owner_version_t newver;
    newver.all = 0;
    newver.version.num = end_time;

    for (OrecList::iterator i = locks.begin(); i != locks.end(); ++i)
      (*i)->v.all = newver.all;
  }

  /**
   * with undo logs, on abort, release locks and increment version numbers by
   * one.  This would be slightly cheaper (in terms of scalability) if we had
   * incarnation numbers
   */
  void releaseAndRevertLocksEager() {
    unsigned long max = 0;

    // increment the version number of each held lock by one
    for (OrecList::iterator i = locks.begin(); i != locks.end(); ++i) {
      owner_version_t newver;
      newver.all = (*i)->p.all;
      newver.version.num++;
      (*i)->v.all = newver.all;
      max = (newver.version.num > max) ? newver.version.num : max;
    }

    // if we bumped a version number to higher than the timestamp, we need to
    // increment the timestamp or else this location could become permanently
    // unreadable
    unsigned ts = timestamp;
    if (max > ts)
      bool_cas(&timestamp, ts, ts+1);
  }

  /*** commit memops and release inevitability */
  void cleanup_inevitable() {
    allocator.onTxCommit();
    common_cleanup();
    inev.onInevCommit();
    inev.onEndTx();
  }

  /*** reset lists and exit epochs */
  void common_cleanup() {
    reads.reset();
    redolog.reset();
    undolog.reset();
    locks.reset();
    inev.onEndTx();
    priv.onEndTx();
  }

  /*** undo any externally visible effects of an aborted transaction */
  void cleanup() {
    // make sure we're aborted
    assert(tx_state == ABORTED);

    if (mode == EagerEager) {
      // run the undo log
      undo();
      // release the locks and bump version numbers
      releaseAndRevertLocksEager();
    }
    else {
      // release the locks and restore version numbers
      releaseAndRevertLocks();
    }

    // undo memory operations
    allocator.onTxAbort();

    // reset all lists
    common_cleanup();
  }

  /*** transactional fence for privatization */
  __attribute__((flatten))
  void priv_fence() {
    priv.Fence();
  }

  /*** log transactional allocations so we can roll them back on abort */
  __attribute__((flatten))
  void* alloc_internal(size_t size) {
    return allocator.txAlloc(size);
  }

  /*** defer transactional frees until commit time */
  __attribute__((flatten))
  void txfree_internal(void* ptr) {
    allocator.txFree(ptr);
  }

  /*** try to become inevitable (must call before any reads/writes) */
  __attribute__((flatten))
  bool try_inev() {
    // multiple calls by an inevitable transaction have no overhead
    if (inev.isInevitable())
      return true;

    // currently, we only support becoming inevitable before performing any
    // transactional reads or writes
    assert((reads.size() == 0) && (redolog.size() == 0) &&
           (undolog.size() == 0));

    return inev.try_inevitable();
  }

  /*** eager/lazy read instrumentation */
  void* stm_read_word_el(void** const addr) {
    // get the orec addr (cast away const)
    volatile orec_t* o = get_orec((void*)addr);
    while (true) {
      // ensure not aborted (we may hold locks...)
      if (tx_state == ABORTED)
        abort();

      // read the orec BEFORE we read anything else
      owner_version_t ovt;
      ovt.all = o->v.all;
      CFENCE;

      // is the location locked?
      if (ovt.version.lock) {
        // if I hold the lock, we already validated this location so I can just
        // read (from writeset or memory) and return
        if (ovt.all == my_lock_word.all) {
          if (redolog.size() != 0) {
            wlog_t* const found = redolog.find((void*)addr);
            if (found != NULL)
              return found->val;
          }
          // not in write set, so read from memory
          return *addr;
        }
        // if I don't hold the lock, call CM if owner is ACTIVE, else spin
        ovt.version.lock = 0;
        if (((ETThread*)ovt.owner)->tx_state != ACTIVE)
          continue;
        WBCM::ConflictResolutions c = cm.onConflict();
        if (c == WBCM::AbortSelf)
          abort();
        if (c == WBCM::Wait)
          continue;
        // try remote abort:
        bool_cas(&(((ETThread*)ovt.owner)->tx_state), ACTIVE, ABORTED);
        continue;
      }
      // if this location is too new, validate
      else if (ovt.version.num > start_time) {
        unsigned newts = timestamp;
        validate();
        start_time = newts;
        continue;
      }

      // orec is unlocked, with ts <= start_time.  read the location
      void* tmp = *addr;

      // postvalidate AFTER reading addr:
      CFENCE;
      if (o->v.all != ovt.all)
        continue;

      // log orec, ensure not aborted, return
      reads.insert((orec_t*)o);
      cm.onOpen();
      if (tx_state == ABORTED)
        abort();
      return tmp;
    }
  }

  /*** eager/eager read instrumentation */
  void* stm_read_word_ee(void** const addr) {
    // get the orec addr (cast away const)
    volatile orec_t* o = get_orec((void*)addr);

    while (true) {
      // ensure not aborted (we may hold locks)
      if (tx_state == ABORTED)
        abort();

      // read the orec BEFORE we read anything else
      owner_version_t ovt;
      ovt.all = o->v.all;
      CFENCE;

      // is the location locked?
      if (ovt.version.lock) {
        // if I hold the lock, we already validated this location so I can just
        // read from memory and return
        if (ovt.all == my_lock_word.all)
          return *addr;

        // if I don't hold the lock, call CM if owner is ACTIVE, else spin
        ovt.version.lock = 0;
        if (((ETThread*)ovt.owner)->tx_state != ACTIVE)
          continue;
        WBCM::ConflictResolutions c = cm.onConflict();
        if (c == WBCM::AbortSelf)
          abort();
        if (c == WBCM::Wait)
          continue;
        // try remote abort:
        bool_cas(&(((ETThread*)ovt.owner)->tx_state), ACTIVE, ABORTED);
        continue;
      }
      // if this location is too new, scale forward
      else if (ovt.version.num > start_time) {
        unsigned newts = timestamp;
        validate();
        start_time = newts;
        continue;
      }

      // orec is unlocked, with ts <= start_time.  read the location
      void* tmp = *addr;

      // postvalidate AFTER reading addr
      CFENCE;
      if (o->v.all != ovt.all)
        continue;

      // log orec, ensure not aborted, return
      reads.insert((orec_t*)o);
      cm.onOpen();
      if (tx_state == ABORTED)
        abort();
      return tmp;
    }
  }

  /*** lazy/lazy read instrumentation */
  void* stm_read_word_ll(void** const addr) {
    // check writeset first
    if (redolog.size() != 0) {
      wlog_t* const found = redolog.find((void*)addr);
      if (found != NULL)
        return found->val;
    }

    // get the orec addr (cast away const)
    volatile orec_t* o = get_orec((void*)addr);

    while (true) {
      // NB: I'm invisible, no need to check if I've been aborted

      // read the orec BEFORE we read anything else
      owner_version_t ovt;
      ovt.all = o->v.all;
      CFENCE;

      // this tx doesn't hold any locks, so if the lock for this addr is held,
      // there is contention
      if (ovt.version.lock) {
        // call CM if owner is ACTIVE, else spin
        ovt.version.lock = 0;
        if (((ETThread*)ovt.owner)->tx_state != ACTIVE)
          continue;
        WBCM::ConflictResolutions c = cm.onConflict();
        if (c == WBCM::AbortSelf)
          abort();
        if (c == WBCM::Wait)
          continue;
        // try remote abort (probably unwise, since lock holder is committing
        // and this txn is in-flight)
        bool_cas(&(((ETThread*)ovt.owner)->tx_state), ACTIVE, ABORTED);
        continue;
      }
      // if this location is too new, scale forward
      else if (ovt.version.num > start_time) {
        unsigned newts = timestamp;
        validate_fast(); // faster, because we don't hold locks
        start_time = newts;
        continue;
      }

      // orec is unlocked, with ts <= start_time.  read the location
      void* tmp = *addr;

      // postvalidate AFTER reading addr:
      CFENCE;
      if (o->v.all != ovt.all)
        continue;

      // log orec and return
      reads.insert((orec_t*)o);
      cm.onOpen();
      // NB: we're invisible, so no need to check tx_state
      return tmp;
    }
  }

  /*** common part of STM write instrumentation: lock a location */
  void acquire(void* addr) {
    // get the orec addr
    volatile orec_t* o = get_orec(addr);

    while (true) {
      // ensure not aborted
      if (tx_state == ABORTED)
        abort();

      // read the orec version number
      owner_version_t ovt;
      ovt.all = o->v.all;

      // if orec free, lock it and put it in lock set
      if (!ovt.version.lock) {
        if (ovt.version.num > start_time) {
          // orec too new.  If we are valid, change our start time to the
          // current timestamp.  Otherwise abort
          unsigned newts = timestamp;
          validate();
          start_time = newts;
          continue;
        }

        // If someone grabs the lock before us, we need to validate or call CM
        if (!bool_cas(&o->v.all, ovt.all, my_lock_word.all))
          continue;

        // we got the lock.  save old version to o->p
        o->p.all = ovt.all;
        // remember that we hold this lock
        locks.insert((orec_t*)o);
      }
      else {
        // if orec is locked but not by us, then there is a conflict
        if (ovt.all != my_lock_word.all) {
          // call CM
          WBCM::ConflictResolutions c = cm.onConflict();
          if (c == WBCM::AbortSelf)
            abort();
          if (c == WBCM::Wait)
            continue;
          // try remote abort:
          ovt.version.lock = 0;
          bool_cas(&((ETThread*)ovt.owner)->tx_state, ACTIVE, ABORTED);
          continue;
        }
      }
      // ensure not aborted
      if (tx_state == ABORTED)
        abort();

      // location is now locked.  return
      return;
    }
  }

  /**
   * Restart a transaction by unwinding mm ops, resetting lists, and doing a
   * longjmp.  We use the same code to abort, restart, or retry.  The 'counter'
   * parameter corresponds to one of the num_? fields of ETThread, the sleep
   * flag lets us give up the CPU briefly before resuming, and the cm_call flag
   * lets us call CM for non-user-initiated abort bookkeeping
   */
  void restart_transaction(unsigned& counter, bool sleep, bool cm_call) {
    // we cannot be inevitable!
    assert(!inev.isInevitable());
    tx_state = ABORTED;
    cleanup();
    if (cm_call)
      cm.onAbort();
    counter++;
    if (sleep)
      sleep_ms(1);
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** increase the abort count and restart the transaction */
  void abort() {
    restart_transaction(num_aborts, false, true);
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<ETThread> Self;

  /*** PUBLIC METHODS:: Thread Management ***/
  static void init(std::string, std::string, bool);
  void dumpstats(unsigned long i);

  /*** PUBLIC METHODS:: Transaction Boundaries ***/

  /*** BeginTransaction increments nesting depth, sets state to ACTIVE */
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
    inev.onBeginTx();

    start_time = timestamp;
    end_time = 0; // so that we know if we need to get a timestamp in
    // order to safely call free()
    // notify CM
    cm.onBegin();
  }

  /*** commit a transaction */
  __attribute__((flatten))
  void commit() {
    // check for remote aborts (unnecessary for LazyLazy)
    if (tx_state == ABORTED)
      abort();

    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth-- > 1)
      return;

    // if inevitable, unwind inevitability and we're done
    // NB: with GRL, we can ignore the timestamp
    if (inev.isInevitable()) {
      num_commits++;
      tx_state = COMMITTED;
      cm.onCommit();
      cleanup_inevitable();
      return;
    }

    // if I don't have writes, I'm committed
    if ((redolog.size() == 0) && (undolog.size() == 0)) {
      num_commits++;
      tx_state = COMMITTED;
      cm.onCommit();
      allocator.onTxCommit();
      common_cleanup();
      return;
    }

    // acquire locks (lazy acquire only)
    if (mode == LazyLazy)
      acquireLocks();

    // we're a writer, so increment the global timestamp
    end_time = 1 + fai(&timestamp);

    // skip validation if nobody else committed
    if (end_time != (start_time + 1))
      validate();

    // set status to committed, abort on failure
    if (!bool_cas(&tx_state, ACTIVE, COMMITTED))
      abort();

    // run the redo log if buffered update
    if (mode != EagerEager)
      redo();

    // release locks
    releaseAndUpdateLocks();

    // notify CM
    cm.onCommit();

    // remember that this was a commit
    num_commits++;

    // commit all frees, reset all lists
    allocator.onTxCommit();
    common_cleanup();
  }

  /*** increase the retry count, sleep, and restart the transaction */
  void retry() {
    restart_transaction(num_retrys, true, false);
  }

  /*** increase the restart count and restart the transaction */
  void restart() {
    restart_transaction(num_restarts, false, false);
  }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read */
  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    // use template magic to transform this (type, address) pair to a set
    // of word-sized reads
    return addr_dispatch<ETThread, T, sizeof(T)>::read(addr, this);
  }

  /*** word-based transactional read, dispatches to appropriate method */
  __attribute__((flatten))
  void* stm_read_word(void** const addr)
  {
    // if inevitable, read inevitably and return
    if (inev.isInevitable())
      return *addr;

    // dispatch based on the mode
    if (mode == EagerEager)
      return stm_read_word_ee(addr);
    else if (mode == EagerLazy)
      return stm_read_word_el(addr);
    else // (mode == LazyLazy)
      return stm_read_word_ll(addr);
  }

  /*** write instrumentation */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    // use template magic to transform this (type, address) pair to a set
    // of word-sized writes
    addr_dispatch<ETThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** transactional write */
  __attribute__((flatten))
  void stm_write_word(void** addr, void* val) {
    // if inevitable, write inevitably and return
    if (inev.isInevitable()) {
      *addr = val;
      return;
    }

    // decide how to lock, log, and update based on the mode
    if (mode == EagerEager) {
      // lock location
      acquire(addr);
      // store old value in undo log
      undolog.insert(wlog_t(addr, *addr));
      // write directly to memory
      *addr = val;
    }
    else if (mode == EagerLazy) {
      // lock location
      acquire(addr);
      // add to redo log
      redolog.insert(wlog_t(addr, val));
    }
    else { // (mode == LazyLazy)
      // add to redo log
      redolog.insert(wlog_t(addr, val));
    }
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }
  static void fence() { Self->priv_fence(); }
  static void acquire_fence() { Self->priv_fence(); }
  static void release_fence() { Self->priv_fence(); }
  static void halt_retry()    { }
  static void setPrio(int i)  { }
  /*** since we only support GRL, both read and write prefetch are nops */
  static bool try_inevitable() { return Self->try_inev(); }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes) { }
}; // class ETThread
} // namespace stm

#endif // ET_HPP
