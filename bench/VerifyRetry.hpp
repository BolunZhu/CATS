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

#ifndef VERIFYRETRY_HPP__
#define VERIFYRETRY_HPP__

#include <stm/stm.hpp>
#include <iostream>
#include "LinkedList.hpp"

using std::cout;
using std::endl;

#define RETRY_ELEMENTS       32

namespace bench
{
  // simple benchmark to call retry and see what happens
  class VerifyRetry : public Benchmark
  {
      // linked list for doing retry tests
      LinkedList retrylist;

    public:
      // constructor initializes the list we use for rety
      VerifyRetry(int _whichtest) : retrylist() { }

      /**
       * Test retry.  This isn't meaningful if you call with only one thread.
       * Even threads fill a buffer, and odd threads drain a buffer, but
       * without any explicit condition variables.  Consequently, we have >1
       * object read to determine whether the thread can do its job or not.
       */
      virtual void random_transaction(thread_args_t* args,
                                      unsigned int*  seed,
                                      unsigned int   val,
                                      int            chance)
      {
          BEGIN_TRANSACTION {
              // we don't want things to keep running once time is up!
              if (!bench::early_tx_terminate) {
                  if (args->id % 2 == 0)
                      FillRetryBuffer();
                  else
                      DrainRetryBuffer();
              }
          } END_TRANSACTION;
      }

      /**
       *  Sanity test for retry: make sure the list is valid, and then ensure
       *  that either the list is empty, the min is >=0 and the max is 64, or
       *  the min is 0 and the max is <= 64.
       */
      virtual bool sanity_check() const
      {
          if (!retrylist.isSane())
              return false;
          int min = retrylist.findmin();
          int max = retrylist.findmax();
          // empty list is sane
          if ((min == -1) && (max == -1))
              return true;
          // if the min is zero, the max needs to be >=0 and <=64
          if ((min == 0) && (max >= 0) && (max <= 64))
              return true;
          // if the max is 64, the min needs to be <=64 and >=0
          if ((max == 64) && (min >= 0) && (min <= 64))
              return true;
          // something must have gone wrong
          return false;
      }

      /**
       *  Wake up anyone who is retrying, via a transaction that touches enough
       *  important stuff to get everyone to wake up
       */
      void wake_retriers() { retrylist.touch_head(); }

      /**
       *  Read the head of the list.  If the list head is null, then insert
       *  zero.  If the head points to zero, then find the max and if the max
       *  is less than 63, insert max+1.  Otherwise, retry.
       */
      void FillRetryBuffer()
      {
          int min = retrylist.findmin();
          // list is empty.  start filling by adding zero
          if (min == -1) {
              retrylist.insert(0);
              return;
          }

          // zero is not in the list, so it must be draining right now
          if (min != 0)
              stm::retry();

          // if the max is RETRY_ELEMENTS, then we're waiting for the list to
          // start draining
          int max = retrylist.findmax();
          if (max == RETRY_ELEMENTS)
              stm::retry();

          // we must be in add mode, so add a new max
          retrylist.insert(max+1);
      }

      /**
       *  Read the head of the list.  If the list head is null, retry.  If the
       *  list head points to zero, and the list contains 63, then remove entry
       *  1.  If the list head points to something other than zero, remove
       *  whatever it points to.
       */
      void DrainRetryBuffer()
      {
          int min = retrylist.findmin();
          // if the list is empty, there is nothing to drain
          if (min == -1)
              stm::retry();

          // if the min is not zero, we're in remove mode, so clobber something
          if (min != 0) {
              retrylist.remove(min);
              return;
          }

          // if the min is zero, see if the list is full and if not, retry
          int max = retrylist.findmax();
          if (max != RETRY_ELEMENTS)
              stm::retry();

          // we must be ready to start removing, so get rid of zero
          retrylist.remove(0);
      }

      // no data structure verification is implemented yet
      virtual bool verify(VerifyLevel_t v) { return true; }
  };
} // namespace bench

#endif // VERIFYRETRY_HPP__
