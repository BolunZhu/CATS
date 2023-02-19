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

#ifndef DLIST_HPP__
#define DLIST_HPP__

#include <stm/stm.hpp>
#include "IntSet.hpp"

namespace bench
{
  // Doubly-Linked List workload
  class DList : public IntSet
  {
      // DNode is a node in the DList
      class DNode : public stm::Object
      {
          GENERATE_FIELD(int, val);
          GENERATE_FIELD(stm::sh_ptr<DNode>, prev);
          GENERATE_FIELD(stm::sh_ptr<DNode>, next);

        public:

          // basic constructor
          DNode(int val = -1) : m_val(val), m_prev(), m_next() { }

          // parameterized constructor for inserts
          DNode(int val,
                const stm::sh_ptr<DNode>& prev,
                const stm::sh_ptr<DNode>& next)
              : m_val(val), m_prev(prev), m_next(next)
          { }
      };

      // the dlist keeps head and tail pointers, for bidirectional traversal
      stm::sh_ptr<DNode> head, tail;

    public:

      DList(int max);

      // insert a node if it doesn't already exist
      virtual void insert(int val);

      // true iff val is in the data structure
      virtual bool lookup(int val) const;

      // remove a node if its value = val
      virtual void remove(int val);

      // make sure the list is in sorted order
      virtual bool isSane() const;

      // print the whole list (assumes isolation)
      virtual void print() const;

      // increment all elements, moving forward
      void increment_forward();

      // increment all elements, moving in reverse
      void increment_backward();

      // increment every seqth element, starting with start, moving forward
      void increment_forward_pattern(int start, int seq);

      // increment every seqth element, starting with start, moving backward
      void increment_backward_pattern(int start, int seq);

      // read the whole list, then increment every element in the chunk
      // starting at chunk_num*chunk_size
      void increment_chunk(int chunk_num, int chunk_size);
  };

} // namespace bench
#endif // DLIST_HPP__
