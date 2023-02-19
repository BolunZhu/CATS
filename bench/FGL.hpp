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

#ifndef FGL_HPP__
#define FGL_HPP__

#include <stm/support/atomic_ops.h>
#include <stm/support/defs.hpp>
#include "IntSet.hpp"

// each node in the list has a private lock, a value, and a next pointer
class FGLNode
{
  public:
    int val;
    FGLNode* next;
    tatas_lock_t lock;

    FGLNode(int val, FGLNode* next) : val(val), next(next), lock(0) { }

    // wrap the lock actions
    inline void acquire() { tatas_acquire(&lock); }
    inline void release() { tatas_release(&lock); }

    void* operator new(size_t size) { return stm::tx_alloc(size); }
    void operator delete(void* p) { stm::mm::txfree(p); }
};

// We construct other data structures from the Linked List; in order to do
// their sanity checks correctly, we might need to pass in a validation
// function of this type
typedef bool (*verifier)(unsigned long, unsigned long);

// the FGL benchmark just implements the four default functions lookup, insert,
// remove, and isSane.
class FGL : public IntSet
{
    // (pass by reference) search the list for val
    virtual bool search(FGLNode *&left, FGLNode *&right, int val) const;

  public:
    // sentinel for the head of the list
    FGLNode* prehead;

    FGL() { prehead = new FGLNode(-1, 0); }

    virtual bool lookup(int val) const;
    virtual void insert(int val);
    virtual void remove(int val);
    virtual bool isSane() const;
    virtual void print() const;

    // make sure the list is in sorted order and for each node x,
    // v(x, verifier_param) is true
    virtual bool extendedSanityCheck(verifier v, unsigned long param) const;
};

#endif // FGL_HPP__
