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

#ifndef RWPATHOLOGY_HPP__
#define RWPATHOLOGY_HPP__

#include <stm/stm.hpp>
#include <iostream>
#include "DList.hpp"

namespace bench
{
  /**
   *  Another benchmark to try to get things to livelock.  here, we never have
   *  write-write conflicts, but we always have read-write conflicts that
   *  hopefully only manifest at validation time in LLT
   */
  class RWPathology : public Benchmark
  {
      // how many elements in the list
      int   ELEMENTS;

      // the actual list
      DList list;

      // the Chunk Size for this experiment
      int   ChunkSize;

    public:
      // constructor: just create a dlist of elements
      RWPathology(int elements, int chunksize)
          : ELEMENTS(elements), list(elements), ChunkSize(chunksize)
      {
          // populate list with all values from 0 to ELEMENTS - 1
          for (int i = 0; i < ELEMENTS; i++)
              list.insert(i);
      }

      // threads traverse the full list, and then increment the counters of all
      // nodes in their "chunk".  They read all other elements, which ensures
      // read/write conflicts.
      //
      // NB: use args->id to get the thread id, which determines the chunk
      virtual void random_transaction(thread_args_t* args,
                                      unsigned int*  seed,
                                      unsigned int   val,
                                      int            chance)
      {
          BEGIN_TRANSACTION {
              // subsumption nesting with conditional to break out of livelock
              // situations when the timer goes off
              if (!bench::early_tx_terminate) {
                  list.increment_chunk(args->id, ChunkSize);
              }
          } END_TRANSACTION;
      }

      // make sure the list is in sorted order
      virtual bool sanity_check() const { return true; }

      // no data structure verification is implemented for the PrivList yet
      virtual bool verify(VerifyLevel_t v) { return true; }
  };

} // namespace bench

#endif // RWPATHOLOGY_HPP__
