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
// Transactional Mutex Lock STM with Buffered Update (TML + Lazy)
//=============================================================================
//   Transactional Mutex Locks runtime.  This offers no write-write
//   concurrency, but very little instrumentation and excellent read
//   concurrency.
//
//   In write acquisition/versioning parlance, this is an lazy-lazy
//   algorithm, without validation support.
//=============================================================================

#ifndef TML_LAZY_HPP
#define TML_LAZY_HPP

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"

namespace stm {

/***  Everything about this STM is encapsulated in its Descriptor */
class TMLLazyThread : public WBTxThread {

  /*** STATIC FIELD DEFINITIONS ***/

  /*** The sequence lock */
  static volatile padded_unsigned_t seqlock;

  /*** PER-INSTANCE FIELD DEFINITIONS */

  // all of the lists of metadata that must be tracked
  RedoLog     writes;
  WBMMPolicy  allocator;

  // for subsumption nesting
  unsigned long nesting_depth;

  // non-throw rollback needs a jump buffer
  jmp_buf* setjmp_buf;

  // cache most recent value of the sequence lock
  unsigned long seq_cache;

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  TMLLazyThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~TMLLazyThread() { }

  /*** run the redo log to commit a transaction */
  void redo() {
    for (RedoLog::iterator i = writes.begin(), e = writes.end(); i != e; ++i)
      *i->addr = i->val;
  }

  /*** common cleanup for transactions that commit */
  void clean_on_commit() {
    allocator.onTxCommit();
    writes.reset();
  }

  /*** unwind a transaction (restart, retry, abort) */
  void restart_transaction(unsigned& counter, bool should_sleep) {
    tx_state = ABORTED;
    ++counter;
    allocator.onTxAbort();
    writes.reset();
    if (should_sleep)
      sleep_ms(1);
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }


  /*** abort the current transaction */
  void abort() { restart_transaction(num_aborts, false); }

  /*** log transactional allocations so we can roll them back on abort */
  void* alloc_internal(size_t size) { return allocator.txAlloc(size); }

  /*** defer transactional frees until commit time */
  void txfree_internal(void* ptr) { allocator.txFree(ptr); }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<TMLLazyThread> Self;

  /*** PUBLIC METHODS:: Thread Management ***/

  static void init(std::string, std::string, bool);
  void dumpstats(unsigned long i);

  /*** PUBLIC METHODS:: Transaction Boundaries ***/

  /*** Begin a transaction */
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
      seq_cache = seqlock.val;
      if ((seq_cache & 1) == 0)
        break;
      spin64();
    }
    ISYNC;
  }

  /*** Commit a transaction */
  __attribute__((flatten))
  void commit() {
    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth-- > 1)
      return;

    // read-only fast path
    if (writes.size() == 0) {
      // log this commit
      tx_state = COMMITTED;
      ++num_commits;
      clean_on_commit();
      return;
    }

    // we have writes... if we can't get the lock, abort
    if (!bool_cas(&seqlock.val, seq_cache, seq_cache + 1))
      abort();

    ISYNC; // WBW; order lock before writeback

    // we're committed... run the redo log, remember that this is a
    // commit
    tx_state = COMMITTED;
    redo();
    ++num_commits;

    LWSYNC; // WBW: order writeback before release

    // release the sequence lock and clean up
    ++seqlock.val;
    clean_on_commit();
  }

  /***  not supported yet */
  void retry() { restart_transaction(num_retrys, true); }
  void restart() { restart_transaction(num_restarts, false); }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    return addr_dispatch<TMLLazyThread, T, sizeof(T)>::read(addr, this);
  }

  /*** read instrumentation */
  inline void* stm_read_word(void** const addr) {
    if (writes.size() != 0) {
      wlog_t* const found = writes.find((void*)addr);
      if (found != NULL)
        return found->val;
    }
    // read the actual value, direct from memory
    void* tmp = *addr;

    // memory fence: read address before looking at the lock
    LWSYNC; CFENCE;

    // if the lock has changed, we must fail
    if (seqlock.val != seq_cache)
      abort();
    return tmp;
  }

  __attribute__((always_inline))
  void stm_write_word(void** addr, void* val) {
    // do a buffered write
    writes.insert(wlog_t(addr, val));
  }

  /*** write instrumentation */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    addr_dispatch<TMLLazyThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  /*** Tml_Lazy provides implicit ALA semantics */
  static void fence() { }
  static void acquire_fence() { }
  static void release_fence() { }

  static void halt_retry()    { }

  static void setPrio(int i)  { }

  /*** Inevitability is not supported yet */
  static bool try_inevitable() { assert(false); return false; }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes)  { }

}; // class stm::TMLLazyThread

} // namespace stm

#endif // TML_LAZY_HPP
