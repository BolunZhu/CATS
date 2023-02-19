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

#include "CGHash.hpp"
#include <iostream>
using std::cout;

// The main hash methods are very straightforward; choose a bucket, then use
// the LinkedList code on that bucket
bool CGHash::lookup(int val) const
{
    FGLNode* head = bucket[val % N_CGHBUCKETS].prehead;
    head->acquire();
    FGLNode* curr = head->next;
    while (curr) {
        if (curr->val > val) {
            head->release();
            return false;
        }
        else if (curr->val == val) {
            head->release();
            return true;
        }
        else
            curr = curr->next;
    }
    head->release();
    return false;
}

void CGHash::insert(int val)
{
    // lock the bucket
    FGLNode* head = bucket[val % N_CGHBUCKETS].prehead;
    head->acquire();

    // search the bucket
    FGLNode* prev = head;
    FGLNode* curr = head->next;
    while (curr) {
        if (curr->val >= val)
            break;
        prev = curr;
        curr = curr->next;
    }

    // insert, unlock, and return
    if (!curr || curr->val > val)
        prev->next = new FGLNode(val, curr);
    head->release();
}

void CGHash::remove(int val)
{
    // lock the bucket
    FGLNode* head = bucket[val % N_CGHBUCKETS].prehead;
    head->acquire();

    // search the bucket
    FGLNode* prev = head;
    FGLNode* curr = head->next;
    while (curr) {
        // item is in list
        if (curr->val == val) {
            prev->next = curr->next;
            delete(curr);
            break;
        }
        // item is not in list
        if (curr->val > val)
            break;
        prev = curr;
        curr = curr->next;
    }
    head->release();
}

// during a sanity check, we want to make sure that every element in a bucket
// actually hashes to that bucket; we do it by passing this method to the
// extendedSanityCheck for the bucket
bool verify_cghash_function(unsigned long val, unsigned long bucket)
{
    return ((val % N_CGHBUCKETS) == bucket);
}

// call extendedSanityCheck() on each bucket (just use the FGL version)
bool CGHash::isSane() const
{
    for (int i = 0; i < N_CGHBUCKETS; i++) {
        if (!bucket[i].extendedSanityCheck(verify_cghash_function, i)) {
            return false;
        }
    }
    return true;
}

// print the hash (just use the FGL version)
void CGHash::print() const
{
    cout << "::CGHash::" << endl;
    for (int i = 0; i < N_CGHBUCKETS; i++) {
        bucket[i].print();
    }
    cout << "::End::" << endl;
}
