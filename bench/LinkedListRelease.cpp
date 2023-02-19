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

#include <stm/stm.hpp>
#include "LinkedListRelease.hpp"

using namespace stm;
using namespace bench;

// constructor just makes a sentinel for the data structure
LinkedListRelease::LinkedListRelease()
    : sentinel(new LLNode())
{ }

// simple sanity check:  make sure all elements of the list are in sorted order
bool LinkedListRelease::isSane(void) const
{
    bool sane = false;

    BEGIN_TRANSACTION;

    sane = true;

    rd_ptr<LLNode> prev(sentinel);
    rd_ptr<LLNode> curr(prev->get_next(prev));

    while (curr != NULL)
    {
        if (prev->get_val(prev) >= curr->get_val(curr))
        {
            sane = false;
            break;
        }

        prev = curr;
        curr = curr->get_next(curr);
    }

    END_TRANSACTION;

    return sane;
}

// insert method; find the right place in the list, add val so that it is in
// sorted order; if val is already in the list, exit without inserting
void LinkedListRelease::insert(int val)
{
    BEGIN_TRANSACTION;
    if (!bench::early_tx_terminate) {

    // traverse the list to find the insert point; release nodes we don't need
    rd_ptr<LLNode> prev(sentinel);
    rd_ptr<LLNode> curr(prev->get_next(prev));

    while (curr != NULL)
    {
        if (curr->get_val(curr) >= val)
            break;

        tx_release(prev);

        prev = curr;
        curr = prev->get_next(prev);
    }

    // now insert new_node between prev and curr
    if (!curr || (curr->get_val(curr) > val)) {
        wr_ptr<LLNode> insert_point(prev);
        insert_point->set_next(sh_ptr<LLNode>(new LLNode(val, curr)),
                               insert_point);
    }
    }
    END_TRANSACTION;
}

// search function
bool LinkedListRelease::lookup(int val) const
{
    bool found = false;

    BEGIN_TRANSACTION;
    if (!bench::early_tx_terminate) {

    rd_ptr<LLNode> prev(sentinel);
    rd_ptr<LLNode> curr(prev->get_next(prev));

    while (curr != NULL)
    {
        if (curr->get_val(curr) >= val)
            break;

        tx_release(prev);

        prev = curr;
        curr = curr->get_next(curr);
    }

    found = ((curr != NULL) && (curr->get_val(curr) == val));
    }
    END_TRANSACTION;

    return found;
}

// remove a node if its value == val
void LinkedListRelease::remove(int val)
{
    BEGIN_TRANSACTION;
    if (!bench::early_tx_terminate) {

    // find the node whose val matches the request
    rd_ptr<LLNode> prev(sentinel);
    rd_ptr<LLNode> curr(prev->get_next(prev));

    while (curr != NULL) {
        // if we find the node, break out of the loop
        if (curr->get_val(curr) >= val)
            break;

        tx_release(prev);

        prev = curr;
        curr = prev->get_next(prev);
    }

    // now check if this is the node we want to delete, and if so remove it
    if ((curr != NULL) && (curr->get_val(curr) == val)) {
        // necessary for consistency
        wr_ptr<LLNode> curr_w(curr);
        wr_ptr<LLNode> prev_w(prev);
        prev_w->set_next(curr_w->get_next(curr_w), prev_w);

        tx_delete(curr);
    }
    }
    END_TRANSACTION;
}
