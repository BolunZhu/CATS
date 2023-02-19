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

#ifndef __CONFLICT_DETECTOR_H__
#define __CONFLICT_DETECTOR_H__

#include "atomic_ops.h"

namespace stm
{
  /**
   *  Policy for implementing the Global Commit Counter validation heuristic
   */
  class GlobalCommitCounterValidationPolicy
  {
      /**
       *  volatile counter of the number of commit attempts up to this point.
       *  We no longer use single-thread optimizations.  Consequently, we
       *  don't have to worry about the initial value of the counter.  All
       *  that matters is that a tx must validate iff the counter value
       *  changes, and that a tx must increment the counter before committing
       *  if it has opened any objects RW.
       */
      static volatile unsigned long
      global_counter  __attribute__ ((aligned(64)));

      /**
       *  Store the last value of the counter that this thread observed
       */
      unsigned long cached_counter;

      /**
       *  Flag that we set if the current tx writes any objects
       */
      bool didRW;
    public:

      /**
       *  Construct the local counter by copying the global counter's current
       *  value and setting didRW false.
       */
      GlobalCommitCounterValidationPolicy()
          : cached_counter(global_counter), didRW(false) { }

      /**
       *  If the counter hasn't changed, we shouldn't validate.  If it has
       *  changed, update the cached_counter and return true.
       */
      bool shouldValidate()
      {
          if (cached_counter == global_counter)
              return false;

          cached_counter = global_counter;
          return true;
      }

      /**
       *  A transaction that wishes to commit can skip final validation if no
       *  other transaction has committed.  Furthermore, a transaction that
       *  has writes must increment the counter.  This method tries to
       *  increment the counter and returns true only if validation can be
       *  skipped.
       */
      bool tryCommit()
      {
          // if we have writes, then return true only if we can increment the
          // counter from the last value we observed.
          if (didRW)
              return bool_cas(&global_counter,
                              cached_counter, cached_counter + 1);
          // if we don't have writes, logic is the same as shouldValidate()
          else
              return (global_counter == cached_counter);
      }

      /**
       *  Reset everything at the beginning of a transaction.
       */
      void onTxBegin()
      {
          cached_counter = global_counter;
          didRW = false;
      }

      /**
       *  If tryCommit() returns false, then I must validate and then call
       *  forceCommit() before I actually commit.  This just ensures that
       *  everyone else knows that I'm going to try to commit.
       */
      void forceCommit()
      {
          if (didRW)
              fai(&global_counter);
      }

      /**
       *  Event method to update metadata if the current tx has writes.
       */
      void onRW() { didRW = true; }

      /**
       *  The global commit counter precludes an optimization for validating
       *  while inserting into the read set.  Runtimes can query their
       *  localcommitcounter to figure out if that optimization is safe.
       */
      const bool isValidatingInsertSafe() { return false; }
  };

  /**
   *  The default validation policy is to do incremental validation every
   *  time we open a new object, and before committing.
   */
  class DefaultValidationPolicy
  {
    public:
      DefaultValidationPolicy() { }
      bool shouldValidate()     { return true; }
      bool tryCommit()          { return false; }
      void forceCommit()        { }
      void onTxBegin()          { }
      void onRW()               { }
      const bool isValidatingInsertSafe() { return true; }
  };

#if defined(STM_COMMIT_COUNTER)
  typedef GlobalCommitCounterValidationPolicy ValidationPolicy;
#elif defined(STM_NO_COMMIT_COUNTER)
  typedef DefaultValidationPolicy ValidationPolicy;
#else
  assert(false); //#error Error: invalid option for COMMIT COUNTER
#endif
} // stm
#endif // __CONFLICT_DETECTOR_H__
