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
// Single Global Lock Atomicity STM (SGLA)
//=============================================================================
//   Timestamp-based STM with single global lock atomicity.  Note that since we
//   don't have sandboxing, we need to use polling to avoid the doomed
//   transaction problem.
//=============================================================================

#ifndef SGLA_HPP
#define SGLA_HPP

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"

namespace stm {
/*** Everything about this STM is encapsulated in its SGLAThread */
class SGLAThread : public OrecWBTxThread {
  /*** STATIC FIELD DEFINITIONS ***/

  /*** The global timestamp */
  static volatile unsigned long timestamp;

  /*** Quiescence tables */
  static padded_unsigned_t numStamps;
  static padded_unsigned_t startStampTable[128];
  static padded_unsigned_t commitStampTable[128];
  static volatile unsigned long linearization_counter;

  /*** PER-INSTANCE FIELD DEFINITIONS ***/

  // for caching the counter start time
  unsigned start_time;

  // for caching the counter end time
  unsigned end_time;

  // for caching the counter last time I looked at it (for avoiding the "doomed
  // transaction" half of the privatization problem)
  unsigned timestamp_cache;

  // all of the lists of metadata that must be tracked
  RedoLog    writes;
  OrecList   reads;
  OrecList   locks;
  WBMMPolicy allocator;

  // for subsumption nesting
  unsigned long nesting_depth;

  // non-throw rollback needs a jump buffer
  jmp_buf* setjmp_buf;

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  // my lock word
  owner_version_t my_lock_word;

  // quiescence support
  unsigned long threadid;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  SGLAThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~SGLAThread() { }

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

  /**
   * validate the read set by making sure that all orecs that we've read have
   * timestamps older than our start time.  call before acquiring
   */
  void validate_fast() {
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
      // if locked or newer than start time, abort
      if (ovt.version.lock || (ovt.version.num > start_time))
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

  /**
   * abort the current transaction by setting state to aborted, undoing all
   * operations, and throwing an exception
   */
  void abort() {
    tx_state = ABORTED;
    cleanup();
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
  }

  /*** undo any externally visible effects of an aborted transaction */
  void cleanup() {
    // make sure we're aborted
    assert(tx_state == ABORTED);

    // release the locks and restore version numbers
    releaseAndRevertLocks();

    // sgla quiescence mechanism
    commitStampTable[threadid].val = 0xFFFFFFFF;
    startStampTable[threadid].val = 0xFFFFFFFF;

    // unroll mm ops
    allocator.onTxAbort();

    // reset all lists
    common_cleanup();
  }

  /*** log transactional allocations so we can roll them back on abort */
  void* alloc_internal(size_t size) { return allocator.txAlloc(size); }

  /*** defer transactional frees until commit time */
  void txfree_internal(void* ptr) { allocator.txFree(ptr); }

  /**
   * an in-flight transaction must make sure it isn't suffering from the
   * "doomed transaction" half of the privatization problem.  We can get that
   * effect by calling this after every transactional read.
   */
  void read_privatization_test() {
    // check the timestamp.  If anyone committed, we need to validate
    unsigned ts = timestamp;
    if (ts == timestamp_cache)
      return;
    ISYNC;

    // we don't hold locks right now
    validate_fast();

    timestamp_cache = ts;
  }

  /*** wait on a timestamp table */
  static void Quiesce(padded_unsigned_t* stampTable, unsigned mynum) {
    unsigned numThreads = numStamps.val;
    for (unsigned id = 0; id < numThreads; ++id) {
      while (stampTable[id].val < mynum)
        yield_cpu();
    }
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<SGLAThread> Self;

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

    start_time = timestamp;
    timestamp_cache = start_time;

    // sgla start linearization
    unsigned mynum = fai(&linearization_counter);
    startStampTable[threadid].val = mynum;

    LWSYNC; // RBR between timestamp and any orecs
    end_time = 0; // so that we know if we need to get a timestamp in
                  // order to safely call free()
  }

  /*** commit a transaction */
  __attribute__((flatten))
  void commit() {
    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth > 1) {
      nesting_depth--;
      return;
    }

    unsigned mynum = startStampTable[threadid].val;
    startStampTable[threadid].val = 0xFFFFFFFF;
    Quiesce(startStampTable, mynum);
    commitStampTable[threadid].val = mynum;

    // if I don't have writes, I'm committed
    if (writes.size() == 0) {
      nesting_depth--;
      num_commits++;
      tx_state = COMMITTED;

      // sgla
      commitStampTable[threadid].val = 0xFFFFFFFF;
      Quiesce(commitStampTable, mynum);

      allocator.onTxCommit();
      common_cleanup();
      return;
    }

    // acquire locks
    acquireLocks();
    ISYNC;

    // increment the global timestamp if we have writes
    end_time = 1 + fai(&timestamp);

    // skip validation if nobody else committed
    if (end_time != (timestamp_cache + 1))
      validate();

    // set status to committed... don't need a cas for now.
    tx_state = COMMITTED;

    // run the redo log
    redo();
    LWSYNC;

    // sgla
    commitStampTable[threadid].val = 0xFFFFFFFF;

    // release locks
    releaseAndIncrementLocks();

    // sgla
    Quiesce(commitStampTable, mynum);

    // remember that this was a commit
    ++num_commits;
    --nesting_depth;

    // commit all frees, reset all lists
    allocator.onTxCommit();
    common_cleanup();
  }

  /**
   * Very simple for now... retry just unwinds the transaction, sleeps, and
   * then unwinds the stack
   */
  void retry() {
    tx_state = ABORTED;
    cleanup();
    ++num_retrys;
    sleep_ms(1);
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** restart: just like retry, except without the sleep */
  void restart() {
    tx_state = ABORTED;
    cleanup();
    ++num_restarts;
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read */
  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    return addr_dispatch<SGLAThread, T, sizeof(T)>::read(addr, this);
  }

  /*** lazy update STM read instrumentation */
  __attribute__((flatten))
  void* stm_read_word(void** const addr) {
    if (writes.size() != 0) {
      wlog_t* const found = writes.find((void*)addr);
      if (found != NULL)
        return found->val;
    }

    // get the orec addr, read the orec's version#
    volatile orec_t* o = get_orec((void*)addr); // cast away const addr
    owner_version_t ovt;
    ovt.all = o->v.all;

    // block if locked
    while (ovt.version.lock) {
      spin128();
      ovt.all = o->v.all;
    }

    // if too new, validate and then scale the timebase
    if (ovt.version.num > start_time) {
      unsigned newts = timestamp;
      validate_fast();
      start_time = newts;
    }
    ISYNC;

    // read the location, no read/read reordering by compiler or CPU
    CFENCE;
    void* tmp = *addr;
    CFENCE;

    LWSYNC;

    // postvalidate orec, abort on failure
    owner_version_t ovt2;
    ovt2.all = o->v.all;

    if (ovt2.all != ovt.all)
      abort();
    ISYNC; // this may not be necessary

    // log orec
    reads.insert((orec_t*)o);

    // privatization safety: avoid the "doomed transaction" half of the
    // privatization problem by polling a global and validating if necessary
    read_privatization_test();
    return tmp;
  }

  /*** lazy update STM write instrumentation */
  __attribute__((flatten))
  void stm_write_word(void** addr, void* val) {
    // record the new value in a redo log
    writes.insert(wlog_t(addr, val));
  }

  /*** instrumented write */
  /*** write instrumentation */
  template <typename T>
  void stm_write(T* addr, T val) {
    addr_dispatch<SGLAThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  /*** no cost to publication/privatization */
  static void fence() { }
  static void acquire_fence() { }
  static void release_fence() { }

  static void halt_retry()    { }

  static void setPrio(int i)  { }

  /*** inevitability is not supported */
  static bool try_inevitable() {
    assert(false && "Inevitability not supported");
    return false;
  }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes)  { }

}; // class stm::SGLAThread
} // namespace stm

#endif // SGLA_HPP
