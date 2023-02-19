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

#ifndef RB_TREE_LARGE_HPP__
#define RB_TREE_LARGE_HPP__

#include <stm/stm.hpp>
#include "IntSet.hpp"

namespace bench
{
    class RBNodeLarge : public stm::Object
    {
        GENERATE_FIELD(Color, color);
        GENERATE_FIELD(int, val);
        // invariant: parent->child[ID] == this
        GENERATE_FIELD(stm::sh_ptr<RBNodeLarge>, parent);
        GENERATE_FIELD(int, ID);
        GENERATE_ARRAY(stm::sh_ptr<RBNodeLarge>, child, 2);
      public:
        // add some padding to make copying expensive
        int pad[1024];

        RBNodeLarge();
    };

    // set of RBNodeLarge objects
    class RBTreeLarge : public IntSet
    {
      public:
        stm::sh_ptr<RBNodeLarge> sentinel;

        RBTreeLarge();

        // standard IntSet methods
        virtual bool lookup(int val) const;
        virtual void insert(int val);
        virtual void remove(int val);
        virtual bool isSane() const; // returns true iff invariants hold

        // for debugging only
        virtual void print(int indent = 0) const;
    };

} // namespace bench

#endif // RB_TREE_LARGE_HPP__
