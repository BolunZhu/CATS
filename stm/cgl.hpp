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
// Coarse Grained Lock STM (CGL)
//=============================================================================
//   No concurrency, no retry support.  This is how transactions would look if
//   we just mapped them to critical sections.
//
//   We have historically used this as a baseline for single threaded overhead
//   testing, but it is often outperformed by TML, so we now recommend using
//   that  runtime for a baseline instead.
//=============================================================================

#ifndef CGL_HPP
#define CGL_HPP

#include "support/atomic_ops.h"
#include "support/defs.hpp"
#include "support/ThreadLocalPointer.hpp"

namespace stm {

class CGLThread {

  /*** The single coarse-grained lock.  Type depends on config.h */
#if defined(STM_LOCK_TATAS)
  static tatas_lock_t globalLock;
#elif defined(STM_LOCK_TICKET)
  static ticket_lock_t globalLock;
#elif defined(STM_LOCK_MCS)
  static mcs_qnode_t* globalLock;
#elif defined(STM_LOCK_PTHREAD) && !defined(_MSC_VER)
  static pthread_mutex_t globalLock;
#elif defined(_MSC_VER)
  // Use a tatas lock for windows default. We should use a Windows lock
  // in the future.
  static tatas_lock_t globalLock;
#else
  // Use pthreads by default on non-windows platforms.
  static pthread_mutex_t globalLock;
#endif

  /*** for subsumption nesting */
  unsigned long nesting_depth;

  /*** for tracking number of commits / aborts (there are no aborts) */
  unsigned num_commits;
  unsigned num_aborts;

#if defined(STM_LOCK_MCS)
  /*** if we're using MCS locks, we need a per-thread mcs_qnode_t */
  mcs_qnode_t mcsnode;
#endif

 public:
  bool isTransactional() { return nesting_depth != 0; }

  void beginTransaction(bool)
  {
    if (nesting_depth++)
      return;
#if defined(STM_LOCK_TATAS)
    tatas_acquire(&globalLock);
#elif defined(STM_LOCK_TICKET)
    ticket_acquire(&globalLock);
#elif defined(STM_LOCK_MCS)
    mcs_acquire(&globalLock, &mcsnode);
#elif defined(STM_LOCK_PTHREAD) && !defined(_MSC_VER)
    pthread_mutex_lock(&globalLock);
#elif defined(_MSC_VER)
    tatas_acquire(&globalLock);
#else
    pthread_mutex_lock(&globalLock);
#endif
  }

  void endTransaction()
  {
    if (--nesting_depth)
      return;
#if defined(STM_LOCK_TATAS)
    tatas_release(&globalLock);
#elif defined(STM_LOCK_TICKET)
    ticket_release(&globalLock);
#elif defined(STM_LOCK_MCS)
    mcs_release(&globalLock, &mcsnode);
#elif defined(STM_LOCK_PTHREAD) && !defined(_MSC_VER)
    pthread_mutex_unlock(&globalLock);
#elif defined(_MSC_VER)
    tatas_release(&globalLock);
#else
    pthread_mutex_unlock(&globalLock);
#endif
    num_commits++;
  }

  /*** Allows comparison of Descriptors. */
  bool operator==(const CGLThread& rhs) const { return (rhs == *this); }
  bool operator!=(const CGLThread& rhs) const { return (rhs != *this); }

  bool try_inevitable() { return true; }
  void dumpStats(unsigned long i);

 private:
  CGLThread() : nesting_depth(0), num_commits(0), num_aborts(0) { }

  ~CGLThread() { }

 public:
  /*** Each thread needs a thread-local pointer to its Descriptor */
  static ThreadLocalPointer<CGLThread> Self;
  static void init() {
    mm::initialize();
    Self.set(new CGLThread());
  }

  //---------------------------------------------------------------------------
  // STAMP Support
  //---------------------------------------------------------------------------
  //   The following routines are needed the STAMP library interface, but are
  //   not used by the RSTM smart pointer interface.
  //
  //   See the $(RSTM)/libstamp directory for more information. STAMP is
  //   available at http://stamp.stanford.edu/.
  //---------------------------------------------------------------------------
  static void commit() { }
  static void restart();  // prints an error and exits.
  static void* alloc(size_t size) { return mm::txalloc(size); }
  static void txfree(void* ptr)   { mm::txfree(ptr); }
  template <typename T> static T stm_read(T* const addr) { return *addr; }
  template <typename T> static void stm_write(T* addr, T val) { *addr = val; }

}; // class CGLThread

} // namespace stm

#endif // CGL_HPP
