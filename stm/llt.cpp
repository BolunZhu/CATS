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

#include "llt.hpp"
#include <iostream>
using std::cout;
using std::endl;
using std::string;

namespace stm
{

/*** Provide backing for the thread-local descriptor (1 of 2) */
ThreadLocalPointer<LLTThread> LLTThread::Self;

#if defined(LOCAL_POINTER_ANNOTATION)
/*** Provide backing for the thread-local descriptor (2 of 2) */
template <> LOCAL_POINTER_ANNOTATION LLTThread*
ThreadLocalPointer<LLTThread>::thr_local_key = NULL;
#endif

#if defined(STM_ROLLBACK_THROW) && defined(_POWER)
volatile unsigned long LLTThread::throw_lock = 0;
#endif

/*** Provide backing for the global timestamp */
volatile unsigned long LLTThread::timestamp = 0;

/*** Inevitability token */
volatile unsigned long LLTThread::inev_token = 0;

/*** Inevitability Bloom Filters*/
#if defined(STM_INEV_BLOOM_SMALL)
Bloom<64, 1> LLTThread::read_filter;
#elif defined(STM_INEV_BLOOM_MEDIUM)
Bloom<4096, 1> LLTThread::read_filter;
#elif defined(STM_INEV_BLOOM_LARGE)
Bloom<4096, 3> LLTThread::read_filter;
#endif

#if defined(STM_INEV_GRL) || defined(STM_INEV_GWLFENCE)
/*** GRL / GWLFence inevitability epoch */
LLTThread::padded_unsigned_t LLTThread::epoch_size = {0};
LLTThread::padded_unsigned_t LLTThread::epoch[128] = {{0}};
#endif

/*** Provide backing for the global privatization metadata */
PrivPolicy::Global PrivPolicy::globals;

/*** dump stats when a thread shuts down */
void LLTThread::dumpstats(unsigned long i)
{
  // mutex lock to serialize I/O
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
void LLTThread::init(string, string, bool)
{
  // initialize mm for this thread
  mm::initialize();

  // create a Descriptor for this thread and save it in thread-local
  // storage
  Self.set(new LLTThread());
}

/*** Constructor just sets up the lists and vars. */
LLTThread::LLTThread()
  : OrecWBTxThread(),
    writes(64), reads(64), locks(64), allocator(),
    write_filter(0),
    nesting_depth(0), is_inevitable(false),
    num_commits(0), num_aborts(0), num_retrys(0), num_restarts(0)
{
#if defined(STM_INEV_GRL) || defined(STM_INEV_GWLFENCE)
  unsigned u = fai(&epoch_size.val);
  epoch_slot = u;
  epoch_buff = (unsigned*)malloc(128 * sizeof(unsigned));
  memset((void*)epoch_buff, 0, 128 * sizeof(unsigned));
#endif
#ifdef STM_INEV_DRAIN
  drain_inev_flag = 0;
#endif
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

