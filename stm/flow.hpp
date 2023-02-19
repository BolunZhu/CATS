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
// Selective Flow Serializability STM (Flow)
//=============================================================================
//  STM runtime supporting either ALA or SFS style publication safety (that is,
//  publication is safe as long as there is a dependence data flow from
//  publisher to reader), and privatization safety via the two counter
//  technique and possibly annotated privatizers.
//=============================================================================


#ifndef FLOW_HPP
#define FLOW_HPP

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"
#include "support/Inevitability.hpp"

namespace stm {
/*** Everything about this STM is encapsulated in its FlowThread */
class FlowThread : public OrecWBTxThread
{
  /*** STATIC FIELD DEFINITIONS ***/

  /*** The global timestamp */
  static volatile unsigned long timestamp;

  /*** The commit completion counter */
  static volatile unsigned long commits;

#if defined(STM_PRIV_SFS)
  /*** The privatization counter */
  static volatile unsigned long privatization_count;
#endif

  /*** PER-INSTANCE FIELD DEFINITIONS ***/

  // for caching the counter start time
  unsigned start_time;

  // for caching the counter end time
  unsigned end_time;

#if defined(STM_PRIV_SFS)
  // for caching the counter last time I looked at it (for avoiding the "doomed
  // transaction" half of the privatization problem)
  unsigned priv_count_cache;
#endif

  // for caching the counter last time I looked at it (for avoiding the "doomed
  // transaction" half of the privatization problem).  If we have to validate
  // at any point, this also lets us avoid final validation
  unsigned timestamp_cache;

  // all of the lists of metadata that must be tracked
  RedoLog   writes;
  OrecList  reads;
  OrecList  locks;
  WBMMPolicy  allocator;

  // for subsumption nesting
  unsigned long nesting_depth;

  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;

  // inevitability support
  InevPolicy inev;

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  // my lock word
  owner_version_t my_lock_word;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  FlowThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~FlowThread() { }

  /**
   * validate the read set by making sure that all orecs that we've read have
   * timestamps older than our start time, unless we locked those orecs.
   */
  void validate() {
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if unlocked and newer than start time, it changed, so abort
      if (!ovt.version.lock && (ovt.version.num > start_time))
        abort();
      // if locked and not by me, abort
      else if (ovt.version.lock && (ovt.all != my_lock_word.all))
        abort();
    }
  }

  /*** lock all locations */
  void acquireLocks() {
    // try to lock every location in the write set
    for (RedoLog::iterator i = writes.begin(), e = writes.end(); i != e; ++i) {
      // get orec, read its version#
      volatile orec_t* o = get_orec((void*)i->addr);
      owner_version_t ovt;
      ovt.all = o->v.all;

      // if orec not locked, lock it and save old to orec.p
      if (!ovt.version.lock) {
        // abort if location changed after I started
        if (ovt.version.num > start_time)
          abort();
        // abort if cannot acquire
        if (!bool_cas(&o->v.all, ovt.all, my_lock_word.all))
          abort();
        // save old version to o->p
        o->p.all = ovt.all;
        // remember that we hold this lock
        locks.insert((orec_t*)o);
      }
      // else if we don't hold the lock abort
      else if (ovt.all != my_lock_word.all)
        abort();
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
    allocator.onTxCommit();
    common_cleanup();
    inev.onInevCommit();
    inev.onEndTx();
  }

  /**
   * abort the current transaction by setting state to aborted, undoing all
   * operations, and throwing an exception
   */
  void abort() {
    assert(!inev.isInevitable());

    tx_state = ABORTED;
    cleanup();
    num_aborts++;

    // if we bumped the timestamp and then aborted during our final validation,
    // then we need to serialize this cleanup wrt concurrent committed writers
    // to support our solution to the deferred update problem
    //
    // There is a tradeoff.  If we do this before calling cleanup(), then we
    // avoid having transactions block in stm_end while we release locks.
    // However, if we release locks first, then we avoid having transactions
    // abort due to encountering locations we've locked
    if (end_time != 0) {
      while (commits < (end_time - 1))
        yield_cpu();
      commits = end_time;
    }

    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** reset lists and exit epochs */
  void common_cleanup() {
    reads.reset();
    writes.reset();
    locks.reset();
    inev.onEndTx();
  }

  /*** undo any externally visible effects of an aborted transaction */
  void cleanup() {
    // make sure we're aborted
    assert(tx_state == ABORTED);

    // release the locks and restore version numbers
    releaseAndRevertLocks();

    // unroll mm ops
    allocator.onTxAbort();

    // reset all lists
    common_cleanup();
  }

  /*** log transactional allocations so we can roll them back on abort */
  void* alloc_internal(size_t size) {
    return allocator.txAlloc(size);
  }

  /*** defer transactional frees until commit time */
  void txfree_internal(void* ptr) {
    allocator.txFree(ptr);
  }

  /*** try to become inevitable (must call before any reads/writes) */
  __attribute__((flatten))
  bool try_inev() {
    // multiple calls by an inevitable transaction have no overhead
    if (inev.isInevitable())
      return true;

    // currently, we only support becoming inevitable before performing
    // any transactional reads or writes
    assert((reads.size() == 0) && (writes.size() == 0));

    return inev.try_inevitable();
  }

  /**
   * an in-flight transaction must make sure it isn't suffering from the
   * "doomed transaction" half of the privatization problem.  We can get that
   * effect by calling this after every transactional read.
   */
  void read_privatization_test() {
#if defined(STM_PRIV_ALA)
    // without annotated privatizers, we poll the timestamp directly to see if
    // we need to validate
    unsigned ts = timestamp;
    if (ts == timestamp_cache)
      return;
    ISYNC;
#elif defined(STM_PRIV_SFS)
    // with annotated privatizers, we poll privatization_count instead
    unsigned pcount = privatization_count;
    if (pcount == priv_count_cache)
      return;
    ISYNC;

    unsigned ts = timestamp;
    LWSYNC; // ugh, rbr, but hey, if it going to save us validation
            // later, it's worth it
#endif
    // optimized validation since we don't hold any locks
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if unlocked and newer than start time, it changed, so abort.
      // if locked, it's not locked by me so abort
      if (ovt.version.lock || (ovt.version.num > start_time))
        abort();
    }

    // remember that we validated at this time
    timestamp_cache = ts;
#if defined(STM_PRIV_SFS)
    priv_count_cache = pcount;
#endif
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<FlowThread> Self;

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

    inev.onBeginTx();

    // get start time, and count of privatizers
    start_time = timestamp;
    timestamp_cache = start_time;
#if defined(STM_PRIV_SFS)
    priv_count_cache = privatization_count;
#endif

    // that this is necessary for privatization by antidependence.  If we don't
    // let all logically committed transactions depart stm_end in order before
    // we begin, and we're a read-only privatizing transaction, then we must
    // ensure that the writer (who did the other half of the privatization) is
    // completely ordered (wrt all writers before it) before we can commit.
    // Ordering the start suffices.  Using this mechanism also lets us use
    // ISYNC instead of LWSYNC on POWER chips
    while (commits < start_time)
      yield_cpu();
    ISYNC;

    end_time = 0; // so that we know if we need to get a timestamp in
                  // order to safely call free()
  }

  /*** commit a transaction */
  __attribute__((flatten))
  void commit() {
    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth-- > 1)
      return;

    // if inevitable, unwind inevitability and we're done
    // NB: with GRL, we can ignore the timestamp and the commits field
    if (inev.isInevitable()) {
      num_commits++;
      tx_state = COMMITTED;
      cleanup_inevitable();
      return;
    }

    // if I don't have writes, I'm committed
    if (writes.size() == 0) {
      num_commits++;
      tx_state = COMMITTED;
      allocator.onTxCommit();
      common_cleanup();
      return;
    }

    // acquire locks
    acquireLocks();
    ISYNC;

    // increment the global timestamp if we have writes
    end_time = 1 + fai(&timestamp);

    // skip validation if nobody else committed since my last validation
    if (end_time != (timestamp_cache + 1))
      validate();

    // set status to committed... don't need a cas for now.
    tx_state = COMMITTED;

    // run the redo log
    redo();
    LWSYNC;

    // release locks
    releaseAndIncrementLocks();

    // remember that this was a commit
    num_commits++;

    // commit all frees, reset all lists
    allocator.onTxCommit();
    common_cleanup();

    // now ensure that transactions depart from stm_end in the order that they
    // incremented the timestamp.  This avoids the "deferred update" half of the
    // privatization problem.
    while (commits != (end_time - 1))
      yield_cpu();

    ISYNC;
    commits = end_time;
  }

  void restart_transaction(unsigned& counter, bool sleep) {
    counter++;
    tx_state = ABORTED;
    cleanup();
    if (sleep)
      sleep_ms(1);
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** increase the retry count, sleep, and restart the transaction */
  void retry() {
    restart_transaction(num_retrys, true);
  }

  /*** restart: just like retry, except without the sleep */
  void restart() {
    restart_transaction(num_restarts, false);
  }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read */
  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    return addr_dispatch<FlowThread, T, sizeof(T)>::read(addr, this);
  }

  /*** lazy update STM read instrumentation */
  __attribute__((flatten))
  void* stm_read_word(void** const addr) {
    // inevitable read instrumentation
    if (inev.isInevitable())
      return *addr;

    // write set lookup
    if (writes.size() != 0) {
      wlog_t* const found = writes.find((void*)addr);
      if (found != NULL)
        return found->val;
    }

    // Unlike LLT and ET, we don't need to prevalidate the orec.  In begin(),
    // we ensured that a start time of X meant that all transactions with start
    // times <=X had finished writeback.  The point of prevalidation is to make
    // sure that we don't read something that someone who committed before we
    // started was still writing back.  Since we have ALA publication safety,
    // we never change start_time.  Coupled with the fact that we did a spin in
    // begin(), prevalidation is unnecessary.

    void* tmp = *addr;
    CFENCE;
    LWSYNC;

    // log orec (cast away const addr)
    volatile orec_t* o = get_orec((void*)addr);
    reads.insert((orec_t*)o);
    owner_version_t ovt;
    ovt.all = o->v.all;
    if (ovt.version.lock || (ovt.version.num > start_time))
      abort();

    // privatization safety: avoid the "doomed transaction" half of the
    // privatization problem by polling a global and validating if necessary
    read_privatization_test();

    // return the value we read
    return tmp;
  }

  /*** lazy update STM write instrumentation */
  __attribute__((flatten))
  void stm_write_word(void** addr, void* val) {
    // write inevitably or write to the redo log
    if (inev.isInevitable())
      *addr = val;
    else
      writes.insert(wlog_t(addr, val));
  }

  /*** instrumented write */
  /*** write instrumentation */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    addr_dispatch<FlowThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  /*** always ALA publication safe, but may have annotated privatizers */
  static void fence() { }
#if defined(STM_PRIV_SFS)
  static void acquire_fence() { fai(&FlowThread::privatization_count); }
#else
  static void acquire_fence() { }
#endif
  static void release_fence() { }

  static void halt_retry()    { }

  static void setPrio(int i)  { }

  /*** GRL inev only */
  static bool try_inevitable() { return Self->try_inev(); }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes) { }
}; // class stm::FlowThread
} // namespace stm

#endif // FLOW_HPP
