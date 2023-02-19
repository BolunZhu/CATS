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

#include "fair.hpp"
#include <iostream>
using std::cout;
using std::endl;
using std::string;

namespace stm
{
/*** Provide backing for the thread-local descriptor (1 of 2) */
ThreadLocalPointer<FairThread> FairThread::Self;

#if defined(LOCAL_POINTER_ANNOTATION)
/*** Provide backing for the thread-local descriptor (2 of 2) */
template <> LOCAL_POINTER_ANNOTATION FairThread*
ThreadLocalPointer<FairThread>::thr_local_key = NULL;
#endif

/*** Provide backing for the global timestamp */
volatile unsigned long FairThread::timestamp = 0;

/*** Provide backing for the global privatization metadata */
PrivPolicy::Global PrivPolicy::globals;

/*** Back all of the CM metadata */
volatile unsigned long BaseCM::inev_token = 0;
volatile unsigned long BaseCM::prioTxCount = 0;
volatile unsigned long BaseCM::retryTxCount = 0;
volatile bool BaseCM::post_on_retry = true;
padded_unsigned_t BaseCM::epoch_size;
padded_unsigned_t BaseCM::epoch[128];
BloomCM::ptr_unsigned_t BloomCM::prioReadSets = {0};
volatile unsigned long VisReadCM::priorities[VisReadCM::num_bits] = {0};
AbstractSchedulerInterface VisReadCM::semaphores[VisReadCM::num_bits];
VisReadCM::rrec VisReadCM::rrecs[rrec_count] = {{{0}}};

/*** dump stats when a thread shuts down */
void FairThread::dumpstats(unsigned long i)
{
  // mutex lock to serialize IO
  static volatile unsigned long mtx = 0;

  while (!bool_cas(&mtx, 0, 1)) { } ISYNC;

  cout << "Thread:" << i
       << "; Commits: "  << num_commits
       << "; Aborts: "   << num_aborts
       << "; Retrys: "   << num_retrys
       << "; Restarts: " << num_restarts
       << endl;

  LWSYNC;
  mtx = 0;
}

/*** Set up a thread's transactional context */
void FairThread::init(string cm_type, string, bool)
{
  // initialize mm for this thread
  mm::initialize();

  // create this thread's Descriptor and save it in thread-local storage
  Self.set(new FairThread(cm_type));
}

/*** Constructor just sets up the lists and vars. */
FairThread::FairThread(string cm_type)
  : OrecWBTxThread(), cm(cm_type),
    writes(64), reads(64), locks(64), allocator(),
    nesting_depth(0),
    num_commits(0), num_aborts(0), num_retrys(0), num_restarts(0)
{
  // compute my lock word
  my_lock_word.owner = this;
  my_lock_word.version.lock = 1;
}

/*** Provide backing for the set of orecs (locks) */
volatile OrecWBTxThread::orec_t
OrecWBTxThread::orecs[OrecWBTxThread::NUM_STRIPES] = {{{0}}};

/*** Provide backing for the allocator */
unsigned long
WBMMPolicyEpoch::trans_nums[MAX_THREADS * WBMMPolicyEpoch::CACHELINE_SIZE] = {0};
volatile unsigned long WBMMPolicyEpoch::thread_count = 0;
} // namespace stm
