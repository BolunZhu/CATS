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

#ifndef STRIDEPATHOLOGY_HPP__
#define STRIDEPATHOLOGY_HPP__

#include <stm/stm.hpp>
#include <iostream>
#include "DList.hpp"

namespace bench
{
  /**
   *  Another benchmark to try to get things to livelock.  here, we don't
   *  usually have write-write conflicts, but we hope to have read-write
   *  conflicts.
   */
  class StridePathology : public Benchmark
  {
      // how many elements in the list
      int   LIVELOCK_ELEMENTS;

      // the actual list
      DList list;

      // the SEQ for this experiment... i.e. increment every SEQth element
      int   SEQ;

    public:
      // constructor: just create a dlist of elements
      StridePathology(int elements, int seq)
          : LIVELOCK_ELEMENTS(elements), list(elements), SEQ(seq)
      {
          // populate list with all values from 0 to LIVELOCK_ELEMENTS - 1
          for (int i = 0; i < LIVELOCK_ELEMENTS; i++)
              list.insert(i);
      }

      // threads either increment from front to back or from back to front,
      // based on ID.  However, threads don't increment everything, so we
      // shouldn't see write-write conflicts, and hopefully we avoid conflicts
      // before validation time
      //
      // NB: use args->id to get the thread id
      virtual void random_transaction(thread_args_t* args,
                                      unsigned int*  seed,
                                      unsigned int   val,
                                      int            action)
      {
          BEGIN_TRANSACTION {
              // subsumption nesting with conditional to break out of livelock
              // situations when the timer goes off
              if (!bench::early_tx_terminate) {
                  if (args->id % 2)
                      list.increment_forward_pattern(args->id, SEQ);
                  else
                      list.increment_backward_pattern(args->id, SEQ);
              }
          } END_TRANSACTION;
      }

      // make sure the list is in sorted order
      virtual bool sanity_check() const { return true; }

      // no data structure verification is implemented for the PrivList yet
      virtual bool verify(VerifyLevel_t v) { return true; }
  };

} // namespace bench

#endif // STRIDEPATHOLOGY_HPP__
