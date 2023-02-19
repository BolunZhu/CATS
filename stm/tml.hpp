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
// Transactional Mutex Locks STM (TML)
//=============================================================================
//   Transactional Mutex Locks runtime.  This offers no write-write
//   concurrency, but very little instrumentation and excellent read
//   concurrency.
//
//   In write acquisition/versioning parlance, this is an eager-eager
//   algorithm.
//=============================================================================

#ifndef TML_HPP
#define TML_HPP

#include <cassert>
#include <string>
#include <setjmp.h>
#include "support/atomic_ops.h"
#include "support/hrtime.h"
#include "support/defs.hpp" // needed for VisualStudio __attribute__ macro
#include "support/word_based_metadata.hpp"
#include "support/ThreadLocalPointer.hpp"

namespace stm {
/***  Everything about this STM is encapsulated in its Descriptor */
class TMLThread {
  /*** STATIC FIELD DEFINITIONS ***/

  /*** The sequence lock */
  static volatile padded_unsigned_t seqlock;

  /*** PER-INSTANCE FIELD DEFINITIONS */

  // track nesting (zero means not in transaction)
  unsigned long nesting_depth;

  // most recent value of the sequence lock (odd means "has the lock")
  unsigned long seq_cache;

  /*** non-throw rollback needs a jump buffer */
  jmp_buf* setjmp_buf;

  // for tracking statistics
  unsigned num_commits;
  unsigned num_aborts;

  /*** PRIVATE METHODS ***/

  /*** private constructor, since we use the init() factory */
  TMLThread();

  /*** destruction is tricky due to MM reclamation... nop for now */
  ~TMLThread() { }

  /**
   * abort the current transaction by setting state to aborted, rolling back,
   * and throwing
   */
  void abort() {
    // change state (we shouldn't have the lock)
    assert((seq_cache & 1) == 0);
    ++num_aborts;

    // unwind stack
    nesting_depth = 0;
    longjmp(*setjmp_buf, 1);
  }

  /*** for simplicity, transactions that allocate cannot abort */
  void* alloc_internal(size_t size) {
    if (nesting_depth > 0)
      try_inevitable();
    return mm::txalloc(size);
  }

  /*** for simplicity, transactions that free cannot abort */
  void txfree_internal(void* ptr) {
    if (nesting_depth > 0)
      try_inevitable();
    mm::txfree(ptr);
  }

  /*** become a writer (inevitable) transaction */
  bool try_inev() {
    // am I a writer already?  If not, try to become one
    if (seq_cache & 1)
      return true;
    if (bool_cas(&seqlock.val, seq_cache, seq_cache + 1)) {
      seq_cache++;
      return true;
    }
    // failure means someone else is a writer, so abort
    abort();
    return false;
  }

 public:

  /*** PUBLIC STATIC FIELDS ***/

  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<TMLThread> Self;

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
    setjmp_buf = buf;

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
  void commit() {
    // don't commit anything if we're nested... just exit this scope
    if (nesting_depth-- > 1)
      return;

    // otherwise remember that this was a commit, maybe release the lock
    ++num_commits;
    if (seq_cache & 1) {
      LWSYNC;
      ++seqlock.val;
    }
  }

  void retry() { assert(false && "Retry() not supported"); }
  void restart() { assert(false && "Restart() not supported"); }

  /*** PUBLIC METHODS:: Per-Access Instrumentation ***/

  /*** instrumented read */
  template <class T>
  inline T stm_read(T* const addr) {
    // read the actual value, direct from memory
    T tmp = *addr;

#ifdef _POWER
    // NB: on POWER, avoiding the LWSYNC is worth the branch
    if (seq_cache & 1)
      return tmp;
#endif
    // memory fence: read address before looking at the lock
    LWSYNC; CFENCE;

    // abort if the lock has changed
    if (seqlock.val != seq_cache)
      abort();

    return tmp;
  }

  /*** write instrumentation */
  template <typename T>
  inline void stm_write(T* addr, T val) {
    // if I have the lock, just write and return
    if (seq_cache & 1) {
      *addr = val;
      return;
    }

    // acquire the lock, abort on failure
    if (!bool_cas(&seqlock.val, seq_cache, seq_cache + 1))
      abort();
    ISYNC;
    // update metadata
    ++seq_cache;

    // do the write
    *addr = val;
  }

  /*** PUBLIC METHODS:: STM API ***/

  static void* alloc(size_t size) { return Self->alloc_internal(size); }
  static void txfree(void* ptr) { Self->txfree_internal(ptr); }

  /*** TML provides implicit ALA semantics */
  static void fence() { }
  static void acquire_fence() { }
  static void release_fence() { }

  static void halt_retry()    { }

  static void setPrio(int i)  { }

  /*** No need for iwp/irp instrumentation */
  static bool try_inevitable() { return Self->try_inev(); }
  static void inev_write_prefetch(const void* addr, unsigned bytes) { }
  static void inev_read_prefetch(const void* addr, unsigned bytes)  { }

  /*** OTHER METHODS ***/

  /***  Allows comparison of Descriptors */
  bool operator==(const TMLThread& rhs) const { return (rhs == *this); }
  bool operator!=(const TMLThread& rhs) const { return (rhs != *this); }
  bool isTransactional() { return nesting_depth > 0; }

}; // class stm::TMLThread

} // namespace stm

#endif // SEQ_HPP
