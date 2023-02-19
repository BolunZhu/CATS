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

#ifndef WWPATHOLOGY_HPP__
#define WWPATHOLOGY_HPP__

#include <stm/stm.hpp>
#include <iostream>
#include "DList.hpp"

namespace bench
{
  /**
   *  Attempt to create livelock via lots of write-write conflicts.  All
   *  transactions conflict, all transactions write to every location, and the
   *  access pattern is bimodal so that we can (hopefully) get conflicts even
   *  in lazy acquire.
   */
  class WWPathology : public Benchmark
  {
      // how many elements in the list
      int   LIVELOCK_ELEMENTS;

      // the actual list
      DList list;

    public:
      // constructor: just create a dlist of elements
      WWPathology(int elements) : LIVELOCK_ELEMENTS(elements), list(elements)
      {
          // populate list with all values from 0 to LIVELOCK_ELEMENTS - 1
          for (int i = 0; i < LIVELOCK_ELEMENTS; i++)
              list.insert(i);
      }

      // threads either increment from front to back or from back to front,
      // based on ID.  This creates lots of conflicts
      //
      // NB: use args->id to get the thread id
      virtual void random_transaction(thread_args_t* args,
                                      unsigned int*  seed,
                                      unsigned int   val,
                                      int            chance)
      {
          BEGIN_TRANSACTION {
              // subsumption nesting with conditional to break out of livelock
              // situations when the timer goes off
              if (!bench::early_tx_terminate) {
                  if (args->id % 2)
                      list.increment_forward();
                  else
                      list.increment_backward();
              }
          } END_TRANSACTION;
      }

      // make sure the list is in sorted order
      virtual bool sanity_check() const { return list.isSane(); }

      // no data structure verification is implemented for the PrivList yet
      virtual bool verify(VerifyLevel_t v) { return true; }
  };
} // namespace bench

#endif // WWPATHOLOGY_HPP__
