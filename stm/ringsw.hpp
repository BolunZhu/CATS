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
// Ring STM Implementation with Single Writer Support (RingSW)
//=============================================================================
//   RingSTM (single writer).  No write-write concurrency, but very little
//   instrumentation and (hopefully) good read concurrency.
//=============================================================================

#ifndef RINGSW_HPP
#define RINGSW_HPP

#include <cassert>
#include <string>
#include <cstring>
#include <setjmp.h>
#include <vector>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"

namespace stm {

/***  Everything about this STM is encapsulated in its Descriptor */
class RingSWThread: public WBTxThread {
  /**
   * Custom Bloom Filter for RingSW. The filter uses a single hash function
   * and is 1024 bits long
   */
  class BloomFilter {
    typedef unsigned bloom_t;
    static const unsigned BLOOML1BITS = sizeof(bloom_t) * 8;
    static const unsigned BLOOML2BITS = BLOOML1BITS * BLOOML1BITS;

    // summary filter (32 bits)
    bloom_t  level_one;
    // big filter (1024 bits)
    bloom_t * level_two;

   public:
    // construct by allocating and zeroing
    BloomFilter() : level_two(new bloom_t[BLOOML1BITS]) { reset(); }

    // destruct by freeing memory
    ~BloomFilter() { delete[] level_two; }

    // copy by swapping level-two filters
    void swap_copy(volatile BloomFilter& right) volatile {
      // no need to swap level one filters, just copy old to new
      level_one = right.level_one;
      // swap level two filters
      bloom_t* swap = level_two;
      level_two = right.level_two;
      right.level_two = swap;
    }

    // add an address to the filter
    void add(const void* u) {
      bloom_t addr_as_bloom = reinterpret_cast<bloom_t> (u);
      unsigned key = (addr_as_bloom >> 3) % BLOOML2BITS;
      unsigned l1bit = key / BLOOML1BITS;
      unsigned l2idx = l1bit;
      unsigned l2bit = key % BLOOML1BITS;

      level_one |= 1UL << l1bit;
      level_two[l2idx] |= 1UL << l2bit;
    }

    // intersect two filters to a bool, where /this/ is volatile
    bool set_intersect(const BloomFilter& with) volatile {
      if (level_one & with.level_one)
        for (unsigned i = 0; i < BLOOML1BITS; ++i)
          if (level_two[i] & with.level_two[i])
            return true;
      return false;
    }

    // reset a filter
    void reset() {
      if (level_one) {
        level_one = 0;
        memset(reinterpret_cast<void *>(level_two), 0, BLOOML2BITS/8);
      }
    }
  };

  /*** the ring consists of these objects */
  struct RingEntry {
    enum RingEntryState { COMPLETE = 0, WRITING = 1 };
    volatile BloomFilter    wf;   // write filter
    volatile RingEntryState st;   // status: WRITING or COMPLETE
    volatile unsigned       ts;   // commit timestamp
    RingEntry() : wf(), st(COMPLETE), ts(0) {
    }
  };

  /*** STATIC FIELD DEFINITIONS */
  static const unsigned             SW_RING_SIZE = 1024;
  static volatile padded_unsigned_t ring_index;     // newest ring entry
  static RingEntry                  ring[SW_RING_SIZE];

  /*** PER-INSTANCE FIELD DEFINITIONS ***/

  // all of the lists of metadata that must be tracked
  RedoLog wset;                 // the buffered writes
  BloomFilter wf;               // write filter
  BloomFilter rf;               // read filter
  WBMMPolicy allocator;
  unsigned nesting_depth;       // for subsumption nesting

  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;

  unsigned start; // logical start time

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  RingSWThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~RingSWThread() { }

  /*** run the redo log to commit a transaction */
  void redo() {
    for (RedoLog::iterator i = wset.begin(), e = wset.end(); i != e; ++i)
      *i->addr = i->val;
  }

  // restart a transaction (abort, retry, restart)
  void restart_transaction(unsigned& counter, bool should_sleep) {
    // change state (we shouldn't have the lock)
    tx_state = ABORTED;
    counter++;

    // reset all lists
    allocator.onTxAbort();
    wset.reset();
    rf.reset();
    wf.reset();

    // sleep?
    if (should_sleep)
      sleep_ms(1);

    // unwind stack
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }


  /*** abort the current transaction */
  void abort() { restart_transaction(num_aborts, false); }

  /*** common cleanup for transactions that commit */
  void clean_on_commit() {
    allocator.onTxCommit();
    wset.reset();
    // clear filters
    rf.reset();
    wf.reset();
  }

  /*** log transactional allocations so we can roll them back on abort */
  void* alloc_internal(size_t size) { return allocator.txAlloc(size); }

  /*** defer transactional frees until commit time */
  void txfree_internal(void* ptr) { allocator.txFree(ptr); }

  /*** check the ring for new entries and validate against them */
  void check() {
    // get the latest ring entry, return if we've seen it already
    unsigned my_index = ring_index.val;
    if (my_index == start)
      return;

    // wait for the latest entry to be initialized
    while (ring[my_index % SW_RING_SIZE].ts < my_index)
      spin64();

    // intersect against all new entries
    for (unsigned i = my_index; i >= start + 1; i--)
      if (ring[i % SW_RING_SIZE].wf.set_intersect(rf))
        abort();

    // wait for newest entry to be writeback-complete before returning
    while (ring[my_index % SW_RING_SIZE].st != RingEntry::COMPLETE)
      spin64();

    // detect ring rollover: start.ts must not have changed
    if (ring[start % SW_RING_SIZE].ts != start)
      abort();

    // ensure this tx doesn't look at this entry again
    start = my_index;
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<RingSWThread> Self;

  /*** PUBLIC METHODS:: Thread Management ***/

  static void init(std::string, std::string, bool);
  void dumpstats(unsigned i);

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

    // get my start point: go back in time 1 entry if latest entry is not
    // writeback-complete (otherwise we'd have to wait)
    start = ring_index.val;
    if ((ring[start % SW_RING_SIZE].st != RingEntry::COMPLETE) ||
        (ring[start % SW_RING_SIZE].ts < start)) {
      --start;
    }
  }

  /*** Commit a transaction */
  __attribute__((flatten))
  void commit() {
    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth-- > 1)
      return;

    // read-only fastpath
    if (wset.size() == 0) {
      tx_state = COMMITTED;
      num_commits++;
      clean_on_commit();
      return;
    }

    // get a commit time, but only succeed in the CAS if this transaction
    // is still valid
    unsigned commit_time;
    do {
      commit_time = ring_index.val;
      check();
    } while (!bool_cas(&ring_index.val, commit_time, (commit_time + 1)));

    // initialize the new ring entry
    ring[(commit_time + 1) % SW_RING_SIZE].st = RingEntry::WRITING;
    ring[(commit_time + 1) % SW_RING_SIZE].wf.swap_copy(wf);
    ring[(commit_time + 1) % SW_RING_SIZE].ts = commit_time + 1;

    // we're committed... run redo log, then mark ring entry COMPLETE
    tx_state = COMMITTED;
    redo();
    ring[(commit_time + 1) % SW_RING_SIZE].st = RingEntry::COMPLETE;

    // clean up
    num_commits++;
    clean_on_commit();
  }

  void retry() { restart_transaction(num_retrys, true); }
  void restart() { restart_transaction(num_restarts, false); }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read dispatch */
  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    return addr_dispatch<RingSWThread, T, sizeof(T)>::read(addr, this);
  }

  /*** instrumented write dispatch */
  template <typename T>
  void stm_write(T* addr, T val) {
    addr_dispatch<RingSWThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** read instrumentation */
  void* stm_read_word(void** const addr) {
    // write set lookup
    if (wset.size() != 0) {
      wlog_t* const found = wset.find((void*)addr);
      if (found != NULL)
        return found->val;
    }

    // read the value from memory
    void* val = *addr;
    CFENCE;

    // add the address to the filter, then validate
    rf.add((void*) addr);
    check();
    return val;
  }

  /*** write instrumentation */
  void stm_write_word(void** addr, void* val) {
    // do a buffered write
    wset.insert(wlog_t(addr, val));
    wf.add(addr);
  }

  /*** PUBLIC METHODS:: STM API ***/

  /*** log transactional allocations so we can roll them back on abort */
  static void* alloc(size_t size) { return Self->alloc_internal(size); }

  /*** defer transactional frees until commit time */
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  /*** implicit privatization safety, no publication safety ***/
  static void fence() { }
  static void acquire_fence() { }
  static void release_fence() { }

  static void halt_retry() { }

  static void setPrio(int i) { }

  /*** inevitability not yet supported */
  static bool try_inevitable() { return false; }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes) { }
}; // class stm::RingSWThread

} // namespace stm

#endif // RINGSW_HPP
