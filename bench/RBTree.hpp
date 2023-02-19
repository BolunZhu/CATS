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

#ifndef RB_TREE_HPP__
#define RB_TREE_HPP__

#include <stm/stm.hpp>
#include "IntSet.hpp"

namespace bench
{
  // set of RBNode objects
  class RBTree : public IntSet
  {
      // Node of an RBTree
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

      // helper functions for sanity checks
      static int blackHeight(const stm::sh_ptr<RBNode>& x);
      static bool redViolation(const stm::rd_ptr<RBNode>& p_r,
                               const stm::sh_ptr<RBNode>& x);
      static bool validParents(const stm::sh_ptr<RBNode>& p, int xID,
                               const stm::sh_ptr<RBNode>& x);
      static bool inOrder(const stm::sh_ptr<RBNode>& x, int lowerBound,
                          int upperBound);
      static void printNode(const stm::sh_ptr<RBNode>& x, int indent = 0);

    public:
      stm::sh_ptr<RBNode> sentinel;

      RBTree();

      // standard IntSet methods
      virtual bool lookup(int val) const;
      virtual void insert(int val);
      virtual void remove(int val);
      virtual bool isSane() const;

      // for debugging only
      virtual void print(int indent = 0) const;
  };

} // namespace bench

#endif // RB_TREE_HPP__
