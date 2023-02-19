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

#include "FGL.hpp"
#include <iostream>
using std::cout;

bool FGL::search(FGLNode *&leftNode, FGLNode *&rightNode, int val) const
{
    leftNode = prehead;
    leftNode->acquire();
    rightNode = leftNode->next;
    while (rightNode) {
        rightNode->acquire();
        if (rightNode->val >= val)
            break;
        leftNode->release();
        leftNode = rightNode;
        rightNode = rightNode->next;
    }
    return rightNode && rightNode->val == val;
}

// look for a node in the list
bool FGL::lookup(int val) const
{
    FGLNode *leftNode, *rightNode;

    bool found = search(leftNode, rightNode, val);
    leftNode->release();
    if (rightNode)
        rightNode->release();
    return found;
}

void FGL::insert(int val)
{
    FGLNode *leftNode, *rightNode;
    bool found = search(leftNode, rightNode, val);
    if (!found) {
        FGLNode* newNode = new FGLNode(val, rightNode);
        leftNode->next = newNode;
    }
    leftNode->release();
    if (rightNode)
        rightNode->release();
}

// remove the node whose value is /val/ from the list
void FGL::remove(int val)
{
    FGLNode *leftNode, *rightNode;
    // if /val/ is in the list, this will return true, and will ensure that
    // rightnode.val will equals val and leftnode.next equals rightnode.
    // rightnode and leftnode will also be acquired
    bool found = search(leftNode, rightNode, val);

    // if we found something, remove rightnode from the list
    if (found) {
        leftNode->next = rightNode->next;
        delete rightNode;
    }
    // otherwise, release rightnode
    else if (rightNode)
        rightNode->release();

    // release leftnode
    leftNode->release();
}

// simple sanity check:  make sure all elements of the list are in sorted order
bool FGL::isSane(void) const
{
    FGLNode* prev = prehead;
    prev->acquire();
    FGLNode* curr = prehead->next;
    while (curr) {
        curr->acquire();
        if (prev->val >= curr->val) {
            prev->release();
            curr->release();
            return false;
        }
        prev->release();
        prev = curr;
        curr = curr->next;
    }
    prev->release();
    return true;
}

// extended sanity check, does the same as the above method, but also calls v()
// on every item in the list
bool FGL::extendedSanityCheck(verifier v, unsigned long v_param) const
{
    FGLNode* prev = prehead;
    prev->acquire();
    FGLNode* curr = prehead->next;
    while (curr) {
        curr->acquire();
        if (prev->val >= curr->val || !v(curr->val, v_param)) {
            prev->release();
            curr->release();
            return false;
        }
        prev->release();
        prev = curr;
        curr = curr->next;
    }
    prev->release();
    return true;
}

// print the list
void FGL::print() const
{
    FGLNode* prev = prehead;
    prev->acquire();
    FGLNode* curr = prehead->next;
    if (curr) {
        cout << "list :: ";

        while (curr) {
            curr->acquire();
            cout << curr->val << "->";
            prev->release();
            prev = curr;
            curr = curr->next;
        }
        cout << "NULL" << endl;
    }
    prev->release();
}
