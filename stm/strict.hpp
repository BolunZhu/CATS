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
// Timestamp Based STM with Strict Serializability (Strict)
//=============================================================================

#ifndef STRICT_HPP
#define STRICT_HPP

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/word_based_descriptor.hpp"
#include "support/ThreadLocalPointer.hpp"
#include "support/WBMMPolicy.hpp"
#include "support/Inevitability.hpp"

namespace stm
{

/*** Everything about this STM is encapsulated in its Descriptor */
class StrictThread : public OrecWBTxThread {
  /*** STATIC FIELD DEFINITIONS ***/

  /*** Token for inevitability */
  static volatile unsigned long inev_token;

  /*** Privatization epoch, for those mechanisms that need a tfence */
#if defined(STM_PRIV_TFENCE)
  static padded_unsigned_t priv_epoch_size;
  static padded_unsigned_t priv_epoch[128];
#endif

  /*** The global timestamp */
  static volatile unsigned long timestamp;

#if defined(STM_PRIV_COMMIT_SERIALIZE)
  /*** The commit serializer */
  static volatile unsigned long commits;

  /*** The privatization counter */
  static volatile unsigned long privatization_count;
#endif

#if defined(STM_PRIV_COMMIT_FENCE)
  static padded_unsigned_t cfence_epoch_size;
  static padded_unsigned_t cfence_epoch[128];
  /*** The privatization counter */
  static volatile unsigned long privatization_count;
#endif

#if defined(STM_PUBLICATION_SHOOTDOWN)
  /*** The publication counter */
  static volatile unsigned long publication_count;
#endif

  /*** PER-INSTANCE FIELD DEFINITIONS ***/

  // for caching the counter start time
  unsigned start_time;

  // for caching the counter end time
  unsigned end_time;

#if defined(STM_PRIV_COMMIT_SERIALIZE) || defined(STM_PRIV_COMMIT_FENCE)
  // for caching the counter last time I looked at it (for avoiding the "doomed
  // transaction" half of the privatization problem)
  unsigned priv_count_cache;
#endif

  // all of the lists of metadata that must be tracked
  RedoLog     writes;
  OrecList    reads;
  OrecList    locks;
  WBMMPolicy  allocator;

  // for subsumption nesting
  unsigned long nesting_depth;

  // non-throw rollback needs a jump buffer
  jmp_buf* setjmp_buf;

  // inevitability support
  InevPolicy inev;

  // privatization support via transactional fence
#if defined(STM_PRIV_TFENCE)
  unsigned  priv_epoch_slot;
  unsigned* priv_epoch_buff;
#endif

#if defined(STM_PRIV_COMMIT_FENCE)
  unsigned cfence_epoch_slot;
  unsigned* cfence_epoch_buff;
#endif

#if defined(STM_PUBLICATION_SHOOTDOWN)
  // publication support via shootdown
  unsigned  pub_count_cache;
#endif

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;
  unsigned num_retrys;
  unsigned num_restarts;

  // my lock word
  owner_version_t my_lock_word;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  StrictThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~StrictThread();

  /**
   * validate the read set by making sure that all orecs that we've read
   * have timestamps older than our start time, unless we locked those
   * orecs.
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

  /*** commit memops and release inevitability */
  void cleanup_inevitable() {
    allocator.onTxCommit();
    common_cleanup();
    inev.onInevCommit();
    inev.onEndTx();
  }

  /**
   * abort the current transaction by setting state to aborted, undoing all
   * operations, and longjumping
   */
  void abort() {
    assert(!inev.isInevitable());

    tx_state = ABORTED;
    cleanup();
    ++num_aborts;

#if defined(STM_PRIV_COMMIT_SERIALIZE)
    // if we bumped the timestamp and then aborted during our final
    // validation, then we need to serialize this cleanup wrt concurrent
    // committed writers to support our solution to the deferred update
    // problem
    //
    // There is a tradeoff.  If we do this before calling cleanup(), then
    // we avoid having transactions block in stm_end while we release
    // locks.  However, if we release locks first, then we avoid having
    // transactions abort due to encountering locations we've locked
    if (end_time != 0) {
      while (commits < (end_time - 1))
        yield_cpu();
      commits = end_time;
    }
#endif

    // unwind stack
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** reset lists and exit epochs */
  void common_cleanup() {
    reads.reset();
    writes.reset();
    locks.reset();
    inev.onEndTx();
#if defined(STM_PRIV_TFENCE)
    priv_epoch[priv_epoch_slot].val++;
#endif
#if defined(STM_PRIV_COMMIT_FENCE)
    if ((cfence_epoch[cfence_epoch_slot].val & 1) == 1)
      cfence_epoch[cfence_epoch_slot].val++;
#endif
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
#if defined(STM_PRIV_TFENCE)
    // copy the current epoch
    unsigned num_txns = priv_epoch_size.val;
    for (unsigned i = 0; i < num_txns; ++i)
      priv_epoch_buff[i] = priv_epoch[i].val;

    // wait for a dominating epoch
    for (unsigned i = 0; i < num_txns; ++i)
      if ((i != priv_epoch_slot) && ((priv_epoch_buff[i] & 1) == 1))
        while (priv_epoch[i].val == priv_epoch_buff[i])
          yield_cpu();

#elif defined(STM_PRIV_COMMIT_FENCE)
    // copy the current epoch
    unsigned num_txns = cfence_epoch_size.val;
    for (unsigned i = 0; i < num_txns; ++i)
      cfence_epoch_buff[i] = cfence_epoch[i].val;

    // wait for a dominating epoch
    for (unsigned i = 0; i < num_txns; ++i)
      if ((i != cfence_epoch_slot) && ((cfence_epoch_buff[i] & 1) == 1))
        while (cfence_epoch[i].val == cfence_epoch_buff[i])
          yield_cpu();
#endif
  }

  /*** log transactional allocations so we can roll them back on abort */
  void* alloc_internal(size_t size) { return allocator.txAlloc(size); }

  /*** defer transactional frees until commit time */
  void txfree_internal(void* ptr) { allocator.txFree(ptr); }

  /*** try to become inevitable (must call before any reads/writes) */
  __attribute__((flatten))
  bool try_inev() {
    // multiple calls by an inevitable transaction have no overhead
    if (inev.isInevitable())
      return true;

    // currently, we only support becoming inevitable before performing any
    // transactional reads or writes
    assert((reads.size() == 0) && (writes.size() == 0));

    return inev.try_inevitable();
  }

#if defined(STM_PRIV_COMMIT_SERIALIZE) || defined(STM_PRIV_COMMIT_FENCE)
  /**
   * an in-flight transaction must make sure it isn't suffering from the
   * "doomed transaction" half of the privatization problem.  We can get that
   * effect by calling this after every transactional read.
   */
  void read_privatization_test() {
    unsigned pcount = privatization_count;
    if (pcount == priv_count_cache)
      return;
    ISYNC;

    unsigned ts = timestamp;
    LWSYNC; // read ts before orecs
    validate_fast();

    priv_count_cache = pcount;
    start_time = ts;
  }
#endif

#if defined(STM_PUBLICATION_SHOOTDOWN)
  /*** force shootdown of concurrent transactions */
  void pub_shootdown() { fai(&publication_count); }
#endif

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<StrictThread> Self;

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

#if defined(STM_PRIV_TFENCE)
    // increment my position in the epoch, without concern about ordering the
    // actual write relative to subsequent operations
    priv_epoch[priv_epoch_slot].val++;
#endif

    inev.onBeginTx();

    start_time = timestamp;
#if defined(STM_PRIV_COMMIT_FENCE)
    priv_count_cache = privatization_count;
#endif
#if defined(STM_PRIV_COMMIT_SERIALIZE)
    // this is necessary for privatization by antidependence.  If we don't let
    // all logically committed transactions depart stm_end in order before we
    // begin, and we're a read-only transaction privatizer, then we must ensure
    // that the writer (who did the other half of the privatization) is
    // completely ordered (wrt all writers before it) before we can commit.
    // Ordering the start suffices.  Using this mechanism also lets us use
    // ISYNC instead of LWSYNC on POWER chips
    priv_count_cache = privatization_count;

    // sort of like the RingSTM suffix property
    while (commits < start_time)
      yield_cpu();

    ISYNC;
#endif

#if defined(STM_PUBLICATION_SHOOTDOWN)
    // get the publication count to see if anyone did a shootdown
    pub_count_cache = publication_count;
#endif

    LWSYNC; // RBR between timestamp and any orecs
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
    if (inev.isInevitable()) {
      ++num_commits;
      tx_state = COMMITTED;
      cleanup_inevitable();
      return;
    }

#if defined(STM_PRIV_COMMIT_FENCE)
    // increment my position in the epoch
    cfence_epoch[cfence_epoch_slot].val++;
    LWSYNC // need wbw ordering
#endif

      // if I don't have writes, I'm committed unless there is publication
      // shootdown
      if (writes.size() == 0) {
#if defined(STM_PUBLICATION_SHOOTDOWN)
        // check for publication shootdown
        if (publication_count != pub_count_cache)
          abort();
#endif
        ++num_commits;
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

    // skip validation if nobody else committed
    if (end_time != (start_time + 1))
      validate();

#if defined(STM_PUBLICATION_SHOOTDOWN)
    // check for publication shootdown
    if (publication_count != pub_count_cache)
      abort();
#endif

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

#if defined(STM_PRIV_COMMIT_SERIALIZE)
    // now ensure that transactions depart from stm_end in the order that they
    // incremend the timestamp.  This avoids the "deferred update" half of the
    // privatization problem.
    while (commits != (end_time - 1))
      yield_cpu();

    ISYNC;
    commits = end_time;
#endif
  }

  /**
   * Very simple for now... retry just unwinds the transaction, sleeps, and
   * then unwinds the stack
   */
  void retry() {
    // we cannot be inevitable!
    assert(!inev.isInevitable());
    tx_state = ABORTED;
    cleanup();
    num_retrys++;
    sleep_ms(1);
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** restart: just like retry, except without the sleep */
  void restart() {
    // we cannot be inevitable!
    assert(!inev.isInevitable());
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
    return addr_dispatch<StrictThread, T, sizeof(T)>::read(addr, this);
  }

  /*** instrumented read */
  void* stm_read_word(void** const addr);

  /*** instrumented write */
  /*** write instrumentation */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    addr_dispatch<StrictThread, T, sizeof(T)>::write(addr, val, this);
  }

  void stm_write_word(void** addr, void* val);

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  static void fence() { Self->priv_fence(); }
  static void acquire_fence() {
#if defined(STM_PRIV_COMMIT_SERIALIZE)
    fai(&privatization_count);
#elif defined (STM_PRIV_COMMIT_FENCE)
    fai(&privatization_count);
    Self->priv_fence();
#else
    Self->priv_fence();
#endif
  }

  static void release_fence() {
#if defined(STM_PUBLICATION_TFENCE)
    Self->priv_fence();
#elif defined(STM_PUBLICATION_SHOOTDOWN)
    Self->pub_shootdown();
#endif
  }

  static void halt_retry()    { }

  static void setPrio(int i)  { }

  static bool try_inevitable() { return Self->try_inev(); }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes)  { }

}; // class stm::StrictThread

/*** lazy update STM read instrumentation */
__attribute__((flatten))
inline void* StrictThread::stm_read_word(void** const addr) {
  // inevitable read instrumentation
  if (inev.isInevitable())
    return *addr;

  if (writes.size() != 0) {
    wlog_t* const found = writes.find((void*)addr);
    if (found != NULL) {
#if defined(STM_PUBLICATION_SHOOTDOWN)
      // check for publication shootdown
      if (publication_count != pub_count_cache)
        abort();
#endif
      return found->val;
    }
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

  // log orec
  reads.insert((orec_t*)o);

#if defined(STM_PUBLICATION_SHOOTDOWN)
  // check for publication shootdown before returning
  if (publication_count != pub_count_cache)
    abort();
#endif

#if defined(STM_PRIV_COMMIT_SERIALIZE) || defined(STM_PRIV_COMMIT_FENCE)
  // privatization safety: avoid the "doomed transaction" half of the
  // privatization problem by polling a global and validating if necessary
  read_privatization_test();
#endif
  return tmp;
}

/*** lazy update STM write instrumentation */
__attribute__((flatten))
inline void StrictThread::stm_write_word(void** addr, void* val) {
  // inevitable write instrumentation
  if (inev.isInevitable()) {
    *addr = val;
    return;
  }

  // record the new value in a redo log
  writes.insert(wlog_t(addr, val));
}

} // namespace stm

#endif // STRICT_HPP
