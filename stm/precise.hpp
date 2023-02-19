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
// Lazy-Lazy, Value-Based Validation, Serialized Commit STM (Precise)
//=============================================================================
//  An orec-free STM.  The STM is opaque, privatization safe, and ALA
//  publication safe. It does not use orecs, instead using JudoSTM-style
// value-based validation. A global sequence lock serializes commits.
//=============================================================================

#ifndef PRECISE_HPP
#define PRECISE_HPP

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"

namespace stm {
class PreciseThread : public WBTxThread
{
  /*** STATIC FIELD DEFINITIONS ***/

  /*** The sequence lock */
  static volatile unsigned long seqlock;

  /*** PER_INSTANCE FIELD DEFINITIONS ***/

  /*** all of the lists of metadata that must be tracked */
  ValueList   reads;
  RedoLog     writes;
  WBMMPolicy  allocator;

  /*** for subsumption nesting */
  unsigned long nesting_depth;

  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;

  /*** cache most recent value of the sequence lock */
  unsigned long seq_cache;

  /*** am I inevitable? */
  bool is_inevitable;

  /*** for tracking statistics */
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  PreciseThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~PreciseThread() { }

  /*** run the redo log to commit a transaction */
  void redo() {
    for (RedoLog::iterator i = writes.begin(), e = writes.end(); i != e; ++i)
      *i->addr = i->val;
  }

  /*** common cleanup for transactions that commit */
  void clean_on_commit() {
    allocator.onTxCommit();
    reads.reset();
    writes.reset();
  }

  /*** commit-time validation (before I get the lock) */
  void validate() {
    while (true) {
      // read the lock
      unsigned s = seqlock;
      // fast exit if no change since last check
      if (s == seq_cache)
        return;
      // re-read until lock is even
      if ((s & 1) == 1)
        continue;
      ISYNC; // RBR between read of seqlock and subsequent validation
      CFENCE;
      // check the read set
      for (ValueList::iterator i = reads.begin(), e = reads.end(); i != e; ++i)
        if (*i->addr != i->val)
          abort();
      CFENCE;
      ISYNC; // RBR between validation and double-check of seqlock
      // restart if seqlock changed during read set iteration
      if (seqlock == s) {
        seq_cache = s;
        break;
      }
    }
  }

  /*** in-flight validation (called from stm_read) */
  bool inflight_validate() {
    unsigned s;
    while (true) {
      // read the lock
      s = seqlock;
      // fast exit if no change since last check
      if (s == seq_cache)
        return true;
      // re-read until lock is even
      if ((s & 1) == 0)
        break;
    }

    ISYNC; // RBR between read of seqlock and subsequent validation
    CFENCE;

    // check the read set
    for (ValueList::iterator i = reads.begin(), e = reads.end(); i != e; ++i)
      if (*i->addr != i->val)
        abort();
    ISYNC;
    if (s == seqlock)
      seq_cache = s;
    return false;
  }

  /**
   * Restart a transaction by unwinding mm ops, resetting lists, and doing a
   * longjmp.  We use the same code to abort, restart, or retry.  The 'counter'
   * parameter corresponds to one of the num_? fields of PreciseThread, and the
   * sleep flag lets us give up the CPU briefly before resuming.
   */
  void restart_transaction(unsigned& counter, bool sleep) {
    counter++;
    tx_state = ABORTED;
    allocator.onTxAbort();
    reads.reset();
    writes.reset();
    nesting_depth = 0;
    if (sleep)
      sleep_ms(1);
    longjmp(*setjmp_buf, 1);
  }

  /*** increase the abort count and restart the transaction */
  void abort() {
    restart_transaction(num_aborts, false);
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
   * become inevitable: this only works if called from the start of a
   * transaction
   */
  void go_inev() {
    if (is_inevitable)
      return;

    // for now, we only support going inevitable before any reads/writes
    assert((reads.size() == 0) && (writes.size() == 0));

    // atomically make seqlock odd
    while (true) {
      unsigned s = seqlock;
      if (s & 1)
        continue;
      if (bool_cas(&seqlock, s, s + 1))
        break;
    }
    is_inevitable = true;
    seq_cache++;
    ISYNC;
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/
  static ThreadLocalPointer<PreciseThread> Self;

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

    // sample the sequence lock until it is even (unheld)
    while (true) {
      seq_cache = seqlock;
      if ((seq_cache & 1) == 0)
        break;
      spin64();
    }
    ISYNC; // order read of seqlock before transaction execution
  }

  /*** Commit a transaction */
  __attribute__((flatten))
  void commit() {
    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth-- > 1)
      return;

    // inevitable path
    if (is_inevitable) {
      tx_state = COMMITTED;
      num_commits++;
      LWSYNC; // order transaction before release
      seqlock = seq_cache + 1;
      is_inevitable = false;
      clean_on_commit();
      return;
    }

    // read-only fast path
    if (writes.size() == 0) {
      tx_state = COMMITTED;
      num_commits++;
      clean_on_commit();
      return;
    }

    // We need to get the lock and validate.  Only attempting to acquire the
    // lock from a valid state minimizes blocking, as in RingSTM
    while (true) {
      if (bool_cas(&seqlock, seq_cache, seq_cache + 1)) {
        seq_cache++;
        break;
      }
      validate();
    }
    ISYNC; // WBW: order lock before writeback

    // success... run the redo log, remember this is a commit
    tx_state = COMMITTED;
    redo();
    num_commits++;

    LWSYNC; // WBW: order writeback before release

    // release the sequence lock and clean up
    seqlock = seq_cache + 1;
    clean_on_commit();
  }

  /*** increase the retry count, sleep, and restart the transaction */
  void retry() {
    restart_transaction(num_retrys, true);
  }

  /*** increase the restart count and restart the transaction */
  void restart() {
    restart_transaction(num_restarts, false);
  }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read */
  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    return addr_dispatch<PreciseThread, T, sizeof(T)>::read(addr, this);
  }

  __attribute__((flatten))
  inline void* stm_read_word(void** const addr) {
    if (is_inevitable)
      return *addr;

    // check the log
    if (writes.size() != 0) {
      wlog_t* const found = writes.find((void*)addr);
      if (found != NULL)
        return found->val;
    }

    while (true) {
      // read the actual value, direct from memory
      CFENCE;
      void* tmp = *addr;
      LWSYNC; CFENCE;

      // validate
      if (inflight_validate()) {
        // postvalidation succeeded, so log read and return the value
        reads.insert(wlog_t(addr, tmp));
        return tmp;
      }
    }
  }

  /*** instrumented write */
  __attribute__((always_inline))
  void stm_write_word(void** addr, void* val) {
    if (is_inevitable) {
      *addr = val;
      return;
    }
    // do a buffered write
    writes.insert(wlog_t(addr, val));
  }

  /*** write instrumentation */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    addr_dispatch<PreciseThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr)   { Self->txfree_internal(ptr); }

  /*** implicit ALA safety */
  static void fence() { }
  static void acquire_fence() { }
  static void release_fence() { }

  /*** no implementation needed since we only use sleep() */
  static void halt_retry()    { }

  static void setPrio(int i)  { }

  static bool try_inevitable() { Self->go_inev(); return true; }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes) { }

}; // class stm::PreciseThread
} // namespace stm

#endif // PRECISE_HPP
