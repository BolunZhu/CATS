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
// Lazy-Lazy-Timestamp STM (LLT)
//=============================================================================


#ifndef LLT_HPP
#define LLT_HPP

#include <cassert>
#include <string>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"
#include "support/Privatization.hpp"

// for some forms of inevitability
#if defined(STM_INEV_BLOOM_SMALL) || defined(STM_INEV_BLOOM_MEDIUM) ||  \
  defined(STM_INEV_BLOOM_LARGE)
#define STM_INEV_BLOOM
#include "support/Bloom.hpp"
#endif

#if defined(STM_ROLLBACK_SETJMP)
#include <setjmp.h>
#endif

namespace stm {
/*** Everything about this STM is encapsulated in its Descriptor */
class LLTThread : public OrecWBTxThread
{
  /*** STATIC FIELD DEFINITIONS ***/

  /**
   *  _POWER targets at URCS do not support exceptions in multithreaded code.
   *  By protecting the entire throw/catch mechanism with a single mutex, we
   *  can preserve correctness, so long as the application does not use
   *  throw/catch for its own purposes.  This does not scale at all, but is
   *  useful for debugging.
   */
#if defined(STM_ROLLBACK_THROW)
#if defined(_POWER)
  static volatile unsigned long throw_lock;
#define BEFORE_THROW { while (!bool_cas(&throw_lock, 0, 1)) { } ISYNC; }
#define AFTER_THROW  { LWSYNC; throw_lock = 0; }
#else // !(_POWER)
#define BEFORE_THROW
#define AFTER_THROW
#endif
#endif

  /*** Token for inevitability */
  static volatile unsigned long inev_token;

  /*** Bloom filter, for those inevitability mechanisms that need it */
#if defined(STM_INEV_BLOOM_SMALL)
  static Bloom<64, 1> read_filter;
#elif defined(STM_INEV_BLOOM_MEDIUM)
  static Bloom<4096, 1> read_filter;
#elif defined(STM_INEV_BLOOM_LARGE)
  static Bloom<4096, 3> read_filter;
#endif

  /*** Inevitability epoch, for those mechanisms that need a tfence. */
#if defined(STM_INEV_GRL) || defined(STM_INEV_GWLFENCE)
  static padded_unsigned_t epoch_size;
  static padded_unsigned_t epoch[128];
#endif

  /*** The global timestamp */
  static volatile unsigned long timestamp;

  /*** PER-INSTANCE FIELD DEFINITIONS ***/

  // for caching the counter start time
  unsigned start_time;

  // for caching the counter end time
  unsigned end_time;

  // all of the lists of metadata that must be tracked
  RedoLog     writes;
  OrecList    reads;
  OrecList    locks;
  WBMMPolicy  allocator;

  // for limiting lookups in write set
  unsigned write_filter;

  // for subsumption nesting
  unsigned long nesting_depth;

#if defined(STM_ROLLBACK_SETJMP)
  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;
#endif

  // inevitability support
  bool is_inevitable;
#if defined(STM_INEV_DRAIN)
  unsigned drain_inev_flag; // did TX increment the drain?
#endif
#if defined(STM_INEV_GRL) || defined(STM_INEV_GWLFENCE)
  unsigned  epoch_slot;
  unsigned* epoch_buff;
#endif

  /*** privatization support */
  PrivPolicy priv;

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  // my lock word
  owner_version_t my_lock_word;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  LLTThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~LLTThread() { }

  /*** inevitable read instrumentation */
  bool inev_read(void* addr);

  /*** inevitable write instrumentation */
  bool inev_write(void* addr);

  /**
   * validate the read set by making sure that all orecs that we've read have
   * timestamps older than our start time, unless we locked those orecs.
   */
  void validate() {
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      // read this orec
      owner_version_t ovt;
      ovt.all = (*i)->v.all;
#if defined(STM_INEV_IRL)
      // ignore reads bit
      ovt.version.reads = 0;
#endif
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
#if defined(STM_INEV_IRL)
        // abort if IRL bit set
        if (ovt.version.reads)
          abort();
#endif
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
#if defined(STM_INEV_GRL)
    // GRL cleanup is easy, since we don't hold locks and nobody else is
    // running
    allocator.onTxCommit();
    common_cleanup();
    inev_token++;
    is_inevitable = false;
    return;
#else
    // if I have writes, get a commit time
    if (locks.size() != 0) {
      end_time = 1 + fai(&timestamp);
    }

#if defined(STM_INEV_IRL)
    // IRL:  use naked writes to release all read locks
    for (OrecList::iterator i = reads.begin(), e = reads.end(); i != e; ++i) {
      if ((*i)->v.version.reads)
        (*i)->v.version.reads = 0;
    }
#endif

#if defined(STM_INEV_BLOOM)
    // clear the bloom filter
    read_filter.reset();
#endif

    // release orecs
    releaseAndIncrementLocks();

    // commit memops, mark self non-inevitable
    allocator.onTxCommit();
    common_cleanup();
    is_inevitable = false;

    // release inevitability
#if defined(STM_INEV_BLOOM) || defined(STM_INEV_GWL) || \
  defined(STM_INEV_GWLFENCE) || defined(STM_INEV_IRL)
    CFENCE; // make sure compiler does not reorder the subsequent write
    inev_token = 0;
#elif defined(STM_INEV_DRAIN)
    while (true) {
      unsigned long f = inev_token;
      if (bool_cas(&inev_token, f, f-1))
        break;
    }
#endif
#endif // GRL
  }

#if defined(STM_INEV_GRL) || defined(STM_INEV_GWLFENCE)
  /*** transactional fence */
  void tfence() {
    // copy the current epoch
    unsigned num_txns = epoch_size.val;
    for (unsigned i = 0; i < num_txns; ++i)
      epoch_buff[i] = epoch[i].val;
#if defined(STM_INEV_GWLFENCE)
    // announce that I've written my epoch
    owner_version_t my1;
    my1.all = inev_token;
    my1.version.lock = 0;
    inev_token = my1.all;
#endif
    // wait for a dominating epoch
    for (unsigned i = 0; i < num_txns; ++i) {
      if ((i != epoch_slot) && ((epoch_buff[i] & 1) == 1)) {
        while (epoch[i].val == epoch_buff[i])
          yield_cpu();
      }
    }
  }
#endif

  /**
   * abort the current transaction by setting state to aborted, undoing all
   * operations, and throwing an exception
   */
  void abort() {
    assert(!is_inevitable);

    tx_state = ABORTED;
    cleanup();
    num_aborts++;

    // unwind stack
#if defined(STM_ROLLBACK_THROW)
    BEFORE_THROW;
    throw RollBack();
#else
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
#endif
  }

  /*** reset lists and exit epochs */
  void common_cleanup() {
    reads.reset();
    writes.reset();
    locks.reset();

#if defined(STM_INEV_GWLFENCE) || defined(STM_INEV_GRL)
    epoch[epoch_slot].val++;
#endif
    priv.onEndTx();

    // clear write filter
    write_filter = 0;
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

#if defined(STM_INEV_DRAIN)
    // if I'm still in the drain, get out
    if (drain_inev_flag) {
      while (true) {
        unsigned long f = inev_token;
        if (bool_cas(&inev_token, f, f-4))
          break; // exit the drain
      }
      drain_inev_flag = false;
    }
#endif
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

 private:
  /**
   *  For STM_INEV_BLOOM_*, this implementation is far from optimal.  The
   *  optimial implementation would combine all inserts into the read filter,
   *  do a single WBR, and then check all orecs.
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
   *  For STM_INEV_BLOOM_*, this implementation is far from optimal.  The
   *  optimial implementation would combine all inserts into the read filter,
   *  do a single WBR, and then check all orecs.
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
    if (is_inevitable)
      return true;

    // currently, we only support becoming inevitable before performing
    // any transactional reads or writes
    assert((reads.size() == 0) && (writes.size() == 0));

#if defined(STM_INEV_NONE)
    // never works
    return false;
#elif defined(STM_INEV_DRAIN)
    // drain needs special code
    while (true) {
      unsigned gInev = inev_token;
      // if someone has the reservation or is inevitable, return
      // false
      if (gInev & 3)
        return false;
      // if the drain is empty, try to grab it
      if (!gInev) {
        if (!bool_cas(&inev_token, 0, 1))
          continue;
        break;
      }
      // if the drain isn't empty, try to reserve it
      if (gInev & ~3) {
        if (!bool_cas(&inev_token, gInev, gInev + 2))
          // didn't get the reservation, so restart
          continue;
        // got the reservation, so wait for an empty drain
        while (inev_token != 2)
          spin64();
        // ok, now move the drain to held
        inev_token = 1;
        break;
      }
    }
    is_inevitable = true;
    return true;
#elif defined(STM_INEV_GRL)
    // GRL: the inev token is a counter
    unsigned old = inev_token;
    if ((old & 1) == 1)
      return false; // token is held
    if (!bool_cas(&inev_token, old, old+1))
      return false; // missed chance to get the token
    is_inevitable = true;
    tfence(); // wait for all active transactions to finish
    return true;
#elif defined(STM_INEV_GWLFENCE)
    // use my lock word as the inev flag.  This lets us use the lsb to indicate
    // whether the inevitable transaction's epoch is initialized or not (via
    // lsb)
    if (!bool_cas(&inev_token, 0, my_lock_word.all))
      return false;
    is_inevitable = true;
    tfence(); // wait for all active transactions to finish

    return true;
#else
    if (!bool_cas(&inev_token, 0, 1))
      return false;
    is_inevitable = true;
    return true;
#endif
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<LLTThread> Self;

  /*** PUBLIC METHODS:: Thread Management ***/

  static void init(std::string, std::string, bool);
  void dumpstats(unsigned long i);

  /*** PUBLIC METHODS:: Transaction Boundaries ***/

  /*** Begin a transaction */
  __attribute__((flatten))
#if defined(STM_ROLLBACK_SETJMP)
  void beginTransaction(jmp_buf* buf) {
#else
  void beginTransaction() {
#endif
    assert(nesting_depth >= 0);
    if (nesting_depth++ != 0)
      return;

    // become active
    tx_state = ACTIVE;
#if defined(STM_ROLLBACK_SETJMP)
    setjmp_buf = buf;
#endif

    allocator.onTxBegin();

#if defined(STM_INEV_GWLFENCE)
    // increment my fence, but that's it
    epoch[epoch_slot].val++;
    WBR; // that write must happen before I read inev flag in stm_end,
    // and before I read the timestamp
#endif

    priv.onBeginTx();

#if defined(STM_INEV_GRL)
    // increment my fence, then block if someone is inevitable
    while (true) {
      // open my current epoch
      epoch[epoch_slot].val++;
      WBR; // write to epoch before reading inev_txn_count
      unsigned old = inev_token;
      // if someone is inevitable, then they might be waiting on me,
      // so get out of their way
      if ((old & 1) == 0)
        break;
      epoch[epoch_slot].val--;
      spin128(); // wait a little while
    }
#endif

    start_time = timestamp;
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

    // if inevitable, unwind inevitability and we're done
    if (is_inevitable) {
      num_commits++;
      nesting_depth--;
      tx_state = COMMITTED;
      cleanup_inevitable();
      return;
    }

    // if I don't have writes, I'm committed
    if (writes.size() == 0) {
      nesting_depth--;
      num_commits++;
      tx_state = COMMITTED;
      allocator.onTxCommit();
      common_cleanup();
      return;
    }

#if defined(STM_INEV_DRAIN)
    // must enter the drain before acquiring orecs
    unsigned gInev;
    while (true) {
      gInev = inev_token;
      // can't enter drain if it is reserved
      if (gInev & 2) {
        spin64();
        continue;
      }
      if (bool_cas(&inev_token, gInev, gInev + 4))
        break;
    }
    drain_inev_flag = true;
    // I'm in the drain.  If there is an active inevitable txn, wait
    // for it to finish
    if ((gInev & 1) != 0)
      while ((inev_token & 1) != 0)
        spin64();
#endif

#if defined(STM_INEV_GWL)
    // if there is an inevitable transaction, then don't acquire
    // anything yet.
    while (inev_token != 0) { spin64(); }
#endif

#if defined(STM_INEV_GWLFENCE)
    // if there is an inevitable transaction waiting on this transaction to
    // finish, or if there is no inevitable transaction, then continue.
    // Otherwise, this transaction should wait until the inevitable transaction
    // commits before attempting to acquire any orecs
    while (true) {
      // read the inevitability flag
      owner_version_t inev_tx;
      inev_tx.all = inev_token;
      // if nobody is inevitable, then we can acquire locks
      if (!inev_tx.all)
        break;
      // if lsb is 1, we need to spin
      if (inev_tx.version.lock) {
        spin64();
        continue;
      }
      // ok, lsb is 0.  compare my epoch value to the inev txn's copy
      // of my epoch
      volatile LLTThread* itx = inev_tx.owner;
      // if inev's timestamp of me == my current epoch slot, break
      if (itx->epoch_buff[epoch_slot] == epoch[epoch_slot].val)
        break;
      spin64();
    }
#endif
    // acquire locks
    acquireLocks();
    ISYNC;

#if defined(STM_INEV_GWL)
    // cannot commit writes when there is an active GWL transaction, because he
    // doesn't check reads
    if (inev_token != 0)
      abort();
#endif
#if defined(STM_INEV_BLOOM)
    // do all bloom filter checks... if any acquired orec is in the filter,
    // abort
    if (inev_token != 0) {
      for (OrecList::iterator i = locks.begin(), e = locks.end(); i != e; ++i)
        if (read_filter.lookup((unsigned)*i))
          abort();
    }
#endif
    // increment the global timestamp if we have writes
    end_time = 1 + fai(&timestamp);

    // skip validation if nobody else committed
    if (end_time != (start_time + 1))
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
    nesting_depth--;

#if defined(STM_INEV_DRAIN)
    // exit the drain
    while (true) {
      unsigned long f = inev_token;
      if (bool_cas(&inev_token, f, f-4))
        break;
    }
    drain_inev_flag = false;
#endif

    // commit all frees, reset all lists
    allocator.onTxCommit();
    common_cleanup();
  }

#if defined(STM_ROLLBACK_THROW)
  /*** call this from catch(...) */
  __attribute__((flatten))
  void onError() {
    if (nesting_depth-- > 1)
      return;
    assert(!is_inevitable);
    tx_state = ABORTED;
    cleanup();
    num_aborts++;
  }

  /*** throw to keep unwinding the stack under subsumption nesting */
  __attribute__((flatten))
  void unwind() {
    // throw unless at outermost level
    if (--nesting_depth > 0)
      throw RollBack();
    AFTER_THROW;
  }
#endif

  /**
   * Very simple for now... retry just unwinds the transaction, sleeps, and
   * then unwinds the stack
   */
  void retry() {
    // we cannot be inevitable!
    assert(!is_inevitable);
    tx_state = ABORTED;
    cleanup();
    num_retrys++;
    sleep_ms(1);

#if defined(STM_ROLLBACK_THROW)
    BEFORE_THROW;
    throw RollBack();
#else
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
#endif
  }

  /*** restart: just like retry, except without the sleep */
  void restart() {
    // we cannot be inevitable!
    assert(!is_inevitable);
    tx_state = ABORTED;
    cleanup();
    num_restarts++;
#if defined(STM_ROLLBACK_THROW)
    BEFORE_THROW;
    throw RollBack();
#else
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
#endif
  }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read */
  template <class T>
  __attribute__((always_inline))
  T stm_read(T* const addr) {
    return addr_dispatch<LLTThread, T, sizeof(T)>::read(addr, this);
  }

  /*** lazy update STM read instrumentation */
  __attribute__((flatten))
  inline void* stm_read_word(void** const addr) {
    // inevitable read instrumentation
    union {
      void** t;
      void* v;
    } a;
    a.t = addr;
    if (inev_read(a.v))
      return *addr;

#if defined(STM_USE_MINIVECTOR_WRITESET)
    // do lookup in write set, we need to iterate in reverse
    // chronological order
    if (write_filter & (1UL<<simplehash((void*)addr))) {
      RedoLog::iterator i = writes.end();
      while (--i >= writes.begin()) {
        if (i->addr == addr)
          return i->val;
      }
    }
#else
    if (writes.size() != 0) {
      wlog_t* const found = writes.find((void*)addr);
      if (found != NULL)
        return found->val;
    }
#endif

    // get the orec addr, read the orec's version#
    volatile orec_t* o = get_orec((void*)addr); // cast away const addr
    owner_version_t ovt;
    ovt.all = o->v.all;

#if defined(STM_INEV_IRL)
    // ignore lock bit
    ovt.version.reads = 0;
#endif
    // abort if locked or too new
    if (ovt.version.lock || (ovt.version.num > start_time))
      abort();

    ISYNC;

    // read the location, no read/read reordering by compiler or CPU
    CFENCE;
    void* tmp = *addr;
    CFENCE;

    LWSYNC;

    // postvalidate orec, abort on failure
    owner_version_t ovt2;
    ovt2.all = o->v.all;
#if defined(STM_INEV_IRL)
    ovt2.version.reads = 0;
#endif
    if (ovt2.all != ovt.all)
      abort();

    // log orec, return value
    reads.insert((orec_t*)o);
    return tmp;
  }

  /*** lazy update STM write instrumentation */
  __attribute__((flatten))
  inline void stm_write_word(void** addr, void* val) {
    // inevitable write instrumentation
    union {
      void** t;
      void* v;
    } a;
    a.t = addr;
    if (inev_write(a.v)) {
      *addr = val;
      return;
    }

    // record the new value in a redo log
    writes.insert(wlog_t(addr, val));

    // and update the bloom filter
    write_filter |= (1UL<<simplehash((void*)addr));
  }

  /*** write instrumentation */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    addr_dispatch<LLTThread, T, sizeof(T)>::write(addr, val, this);
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  static void fence() { Self->priv_fence(); }
  static void acquire_fence() { Self->priv_fence(); }
  static void release_fence() { Self->priv_fence(); }

  static void halt_retry()    { }

  static void setPrio(int i)  { }

  static bool try_inevitable() { return Self->try_inev(); }
  static void inev_write_prefetch(const void* addr, unsigned bytes)
  {
    Self->inevitable_write_prefetch(addr, bytes);
  }
  static void inev_read_prefetch(const void* addr, unsigned bytes)
  {
    Self->inevitable_read_prefetch(addr, bytes);
  }

}; // class LLTThread

/*** inevitable read instrumentation */
inline bool LLTThread::inev_read(void* addr) {
  // if not inevitable, fall out to standard read code
  if (!is_inevitable)
    return false;

#if defined(STM_INEV_NONE)
  assert(false);
  return false;
#endif

#if defined(STM_INEV_DRAIN) || defined(STM_INEV_GWLFENCE) || \
  defined(STM_INEV_GRL)
  // we've got a contract guaranteeing that nobody is holding a lock on
  // this location, so we can just read it directly
  return true;
#else
  // read the orec
  volatile orec_t* o = get_orec(addr);
  owner_version_t ovt;
  ovt.all = o->v.all;

#if defined(STM_INEV_IRL)
  // if I've got a read lock, return immediately
  if (ovt.version.reads == 1)
    return true;
#endif
  // if I've got a write lock, return immediately
  if (ovt.all == my_lock_word.all)
    return true;

#if defined(STM_INEV_IRL)
  // need to acquire a read lock
  while (true) {
    ovt.all = o->v.all;
    // if lock held, wait for committing tx to finish
    if (ovt.version.lock) {
      spin64();
      continue;
    }
    owner_version_t readlock;
    readlock.all = ovt.all;
    readlock.version.reads = 1;
    if (bool_cas(&o->v.all, ovt.all, readlock.all))
      break;
  }
  // need to log the read lock so it can be released later
  reads.insert(const_cast<orec_t*>(o)); // cast away volatile
  return true;
#endif

#if defined(STM_INEV_GWL)
  // if it is locked, wait for the lock holder to abort
  while (o->v.version.lock)
    spin64();
  return true;
#endif

#if defined(STM_INEV_BLOOM)
  // need to put location in bloom filter and then ensure no concurrent
  // lock
  read_filter.insert((unsigned)o);
  WBR;
  while (o->v.version.lock)
    spin64();
  return true;
#endif
#endif
}

/*** inevitable write instrumentation */
inline bool LLTThread::inev_write(void* addr)
{
  // if not inevitable, fall out to standard write code
  if (!is_inevitable)
    return false;
#if defined(STM_INEV_GRL)
  // no instrumentation needed
  return true;
#endif

  // need to get a write lock on this location

  // first get the orec
  volatile orec_t* o = get_orec(addr);
  owner_version_t ovt;
  ovt.all = o->v.all;

  // if I already have the lock, return
  if (ovt.all == my_lock_word.all)
    return true;

  // if IRL and I have a read lock, upgrade with a standard write,
  // bookkeep the lock, and return
#if defined(STM_INEV_IRL)
  if (ovt.version.reads == 1) {
    // erase read lock bit
    ovt.version.reads = 0;
    // write lock, update o->p (not strictly required)
    o->v.all = my_lock_word.all;
    o->p.all = ovt.all;
    // bookkeep the lock
    locks.insert(const_cast<orec_t*>(o)); // cast away volatile
    return true;
  }
#endif
#if defined(STM_INEV_DRAIN) || defined(STM_INEV_GWLFENCE)
  // drain can acquire with a standard write
  o->v.all = my_lock_word.all;
  o->p.all = ovt.all;
  // remember the lock, return
  locks.insert(const_cast<orec_t*>(o)); // cast away volatile
  return true;
#else
  // if not drain, then need to cas to acquire, retry until success
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
#endif
}
} // namespace stm

#endif // LLT_HPP
