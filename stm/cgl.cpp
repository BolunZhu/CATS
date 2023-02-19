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

#include "cgl.hpp"
#include <iostream>
using std::cout;
using std::endl;

namespace stm {
/*** Provide backing for the thread-local descriptor (1 of 2) */
ThreadLocalPointer<CGLThread> CGLThread::Self;

#if defined(LOCAL_POINTER_ANNOTATION)
/*** Provide backing for the thread-local descriptor (2 of 2) */
template <> LOCAL_POINTER_ANNOTATION CGLThread*
ThreadLocalPointer<CGLThread>::thr_local_key = NULL;
#endif

/*** provide backing for the mutex lock */
#if defined(STM_LOCK_TATAS)
tatas_lock_t CGLThread::globalLock __attribute__ ((aligned(64))) = 0;
#elif defined(STM_LOCK_TICKET)
ticket_lock_t CGLThread::globalLock __attribute__ ((aligned(64))) = {0};
#elif defined(STM_LOCK_MCS)
mcs_qnode_t* CGLThread::globalLock __attribute__ ((aligned(64))) = NULL;
#elif defined(STM_LOCK_PTHREAD) && !defined(_MSC_VER)
pthread_mutex_t CGLThread::globalLock __attribute__ ((aligned(64))) = PTHREAD_MUTEX_INITIALIZER;
#elif defined(_MSC_VER)
tatas_lock_t CGLThread::globalLock __attribute__ ((aligned(64))) = 0;
#else
pthread_mutex_t CGLThread::globalLock __attribute__ ((aligned(64))) = PTHREAD_MUTEX_INITIALIZER;
#endif

void CGLThread::dumpStats(unsigned long i)
{
  static volatile unsigned long mtx = 0;
  while (!bool_cas(&mtx, 0, 1)) { }
  ISYNC;

  cout << "Thread:" << i
       << "; Commits: " << num_commits << "; Aborts: " << num_aborts
       << endl;

  LWSYNC;
  mtx = 0;
}

// Supports the STAMP library interface.
void CGLThread::restart() {
  std::cerr << "Restarting is not supported when using the CGL library\n";
  __builtin_exit(1);
}

} // namespace stm
