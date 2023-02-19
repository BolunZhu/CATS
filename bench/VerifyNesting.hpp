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

#ifndef VERIFYNESTING_HPP__
#define VERIFYNESTING_HPP__

#include <stm/stm.hpp>
#include <iostream>
#include "LinkedList.hpp"

using std::cout;
using std::endl;

#define SUBSUMPTION_ELEMENTS 64

namespace bench
{
  // simple test to make sure subsumption nesting works
  class VerifyNesting : public Benchmark
  {
      // two linked lists for doing subsumption tests
      LinkedList subslist1;
      LinkedList subslist2;

    public:
      // constructor sets us the lists for subsumption tests
      VerifyNesting(int _whichtest) : subslist1(), subslist2()
      {
          // populate subsumption list 1 with all values from 0 to 63
          for (int i = 0; i < SUBSUMPTION_ELEMENTS; i++)
              subslist1.insert(i);
      }

      /**
       *  The point here is that without subsumption, there's a risk that
       *  we'll get inconsistencies in the sanity check.  The invariant is
       *  that the two lists have a null intersection, but their union is the
       *  set 0..63.  We're also making sure that aborts are handled
       *  correctly, since the subsumed transactions abort.
       *
       *  NB: it's not until the sanity test runs that you'll know that this
       *  actually did the right thing
       */
      virtual void random_transaction(thread_args_t* args,
                                      unsigned int*  seed,
                                      unsigned int   val,
                                      int            chance)
      {
          BEGIN_TRANSACTION {
              if (subslist1.lookup(val)) {
                  subslist1.remove(val);
                  subslist2.insert(val);
              }
              else {
                  subslist1.insert(val);
                  subslist2.remove(val);
              }
          } END_TRANSACTION;
      }

      /**
       *  Sanity test for subsumption test: make sure that lists are valid,
       *  that their union is 0..63, and that their intersection is empty
       */
      virtual bool sanity_check() const
      {
          if (!subslist1.isSane())
              return false;
          if (!subslist2.isSane())
              return false;
          for (int i = 0; i < SUBSUMPTION_ELEMENTS; i++) {
              bool in1 = subslist1.lookup(i);
              bool in2 = subslist2.lookup(i);
              if (in1 == in2)
                  return false;
          }
          return true;
      }

      // no data structure verification is implemented yet
      virtual bool verify(VerifyLevel_t v) { return true; }
  };
} // namespace bench

#endif // VERIFYNESTING_HPP__
