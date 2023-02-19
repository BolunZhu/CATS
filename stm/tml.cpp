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

#include "tml.hpp"
#include <iostream>
using std::cout;
using std::endl;
using std::string;

namespace stm {

/*** Provide backing for the thread-local descriptor (1 of 2) */
ThreadLocalPointer<TMLThread> TMLThread::Self;

#if defined(LOCAL_POINTER_ANNOTATION)
/*** Provide backing for the thread-local descriptor (2 of 2) */
template <> LOCAL_POINTER_ANNOTATION TMLThread*
ThreadLocalPointer<TMLThread>::thr_local_key = NULL;
#endif

/*** Provide backing for the sequence lock */
volatile padded_unsigned_t TMLThread::seqlock = {0};

/*** dump stats when a thread shuts down */
void TMLThread::dumpstats(unsigned long i) {
  // mutex lock to serialize I/O
  static volatile unsigned long mtx = 0;

  while (!bool_cas(&mtx, 0, 1)) { } ISYNC;

  cout << "Thread:" << i
       << "; Commits: "  << num_commits
       << "; Aborts: "   << num_aborts
       << endl;

  LWSYNC;
  mtx = 0;
}

/*** Set up a thread's transactional context */
void TMLThread::init(string, string, bool) {
  // initialize mm for this thread
  mm::initialize();

  // create a Descriptor and save it in thread-local storage
  Self.set(new TMLThread());
}

/*** Constructor just zeroes everything */
TMLThread::TMLThread()
  : nesting_depth(0), seq_cache(0), num_commits(0), num_aborts(0)
{ }

} // namespace stm
