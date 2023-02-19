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

#ifndef PRIVTREE_HPP__
#define PRIVTREE_HPP__

#include <stm/stm.hpp>
#include "Benchmark.hpp"
#include "Stack.hpp"

namespace bench
{
  // this benchmark builds RBTrees via transactions, and then privatizes those
  // trees and gives them to another thread (via a stack).  The other thread
  // privately computes the sum and average values of the tree, and then frees
  // the tree.
  class PrivTree : public Benchmark
  {
      // we're going to overload Color so that we can use it during
      // privatized traversal
      enum Color { RED, BLACK, LEFT, RIGHT };

      // Node of an PrivTree
      class RBNode : public stm::Object
      {
          GENERATE_FIELD(Color, color);
          GENERATE_FIELD(int, val);
          // invariant: parent->child[ID] == this
          GENERATE_FIELD(stm::sh_ptr<RBNode>, parent);
          GENERATE_FIELD(int, ID);
          GENERATE_ARRAY(stm::sh_ptr<RBNode>, child, 2);
        public:
          // basic constructor
          RBNode(Color color = BLACK,
                 long val = -1,
                 stm::sh_ptr<RBNode> parent = stm::sh_ptr<RBNode>(NULL),
                 long ID = 0,
                 stm::sh_ptr<RBNode> child0 = stm::sh_ptr<RBNode>(NULL),
                 stm::sh_ptr<RBNode> child1 = stm::sh_ptr<RBNode>(NULL))
              : m_color(color), m_val(val), m_parent(parent), m_ID(ID)
          {
              m_child[0] = child0;
              m_child[1] = child1;
          }
      };

      stm::sh_ptr<RBNode> sentinel;
      int m_max;
      int m_producers;

      TxStack<stm::sh_ptr<RBNode> >* STACK;

      // helper functions for sanity checks
      static int blackHeight(const stm::sh_ptr<RBNode>& x);
      static bool redViolation(const stm::rd_ptr<RBNode>& p_r,
                               const stm::sh_ptr<RBNode>& x);
      static bool validParents(const stm::sh_ptr<RBNode>& p, int xID,
                               const stm::sh_ptr<RBNode>& x);
      static bool inOrder(const stm::sh_ptr<RBNode>& x, int lowerBound,
                          int upperBound);
      static void printNode(const stm::sh_ptr<RBNode>& x, int indent = 0);

      void consumer();

      // standard IntSet methods
      bool lookup(int val) const;
      void insert(int val);
      void remove(int val);

    public:
      PrivTree(int _max, int _producers);
      virtual bool sanity_check() const;

      virtual bool verify(VerifyLevel_t v) { return true; }

      virtual void random_transaction(thread_args_t* args,
                                      unsigned int* seed,
                                      unsigned int val,
                                      int chance)
        {
            // am I a producer thread?
            if (args->id < m_producers) {
                unsigned int j = val % m_max;
                int action = chance;

                if (action < BMCONFIG.lookupPct) {
                    if (lookup(j)) {
                        ++args->count[TXN_LOOKUP_TRUE];
                    }
                    else {
                        ++args->count[TXN_LOOKUP_FALSE];
                    }
                }
                else if (action < BMCONFIG.insertPct) {
                    insert(j);
                    ++args->count[TXN_INSERT];
                }
                else {
                    remove(j);
                    ++args->count[TXN_REMOVE];
                }
            }
            else {
                consumer();
            }
        }

      // for debugging only
      virtual void print(int indent = 0) const;
  };

} // namespace bench

#endif // PRIVTREE_HPP__
