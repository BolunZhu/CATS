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

#include <iostream>

#include "LinkedList.hpp"

using namespace stm;
using namespace bench;

// constructor just makes a sentinel for the data structure
LinkedList::LinkedList() : sentinel(new LLNode()) { }

// simple sanity check:  make sure all elements of the list are in sorted order
bool LinkedList::isSane(void) const
{
    bool sane = false;
    BEGIN_TRANSACTION {
        sane = true;
        rd_ptr<LLNode> prev(sentinel);
        rd_ptr<LLNode> curr(prev->get_next(prev));

        while (curr != NULL) {
            if (prev->get_val(prev) >= curr->get_val(curr)) {
                sane = false;
                break;
            }
            prev = curr;
            curr = curr->get_next(curr);
        }
    } END_TRANSACTION;
    return sane;
}

// extended sanity check, does the same as the above method, but also calls v()
// on every item in the list
bool LinkedList::extendedSanityCheck(verifier v, unsigned long v_param) const
{
    bool sane = false;
    BEGIN_TRANSACTION {
        sane = true;
        rd_ptr<LLNode> prev(sentinel);
        rd_ptr<LLNode> curr(prev->get_next(prev));
        while (curr != NULL) {
            if (!v(curr->get_val(curr), v_param) ||
                (prev->get_val(prev) >= curr->get_val(curr)))
            {
                sane = false;
                break;
            }
            prev = curr;
            curr = prev->get_next(prev);
        }
    } END_TRANSACTION;
    return sane;
}

// insert method; find the right place in the list, add val so that it is in
// sorted order; if val is already in the list, exit without inserting
void LinkedList::insert(int val)
{
    BEGIN_TRANSACTION {
        if (!bench::early_tx_terminate) {
        // traverse the list to find the insertion point
        rd_ptr<LLNode> prev(sentinel);
        rd_ptr<LLNode> curr(prev->get_next(prev));

        while (curr != NULL) {
            if (curr->get_val(curr) >= val)
                break;
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
    } END_TRANSACTION;
}


// search function
bool LinkedList::lookup(int val) const
{
    bool found = false;

    BEGIN_READONLY_TRANSACTION {
        if (!bench::early_tx_terminate) {
        rd_ptr<LLNode> curr(sentinel);
        curr = curr->get_next(curr);

        while (curr != NULL) {
            if (curr->get_val(curr) >= val)
                break;
            curr = curr->get_next(curr);
        }

        found = ((curr != NULL) && (curr->get_val(curr) == val));
        }
    } END_TRANSACTION;

    return found;
}

// findmax function
int LinkedList::findmax() const
{
    int max = -1;
    BEGIN_TRANSACTION {
        rd_ptr<LLNode> curr(sentinel);
        while (curr != NULL) {
            max = curr->get_val(curr);
            curr = curr->get_next(curr);
        }
    } END_TRANSACTION;
    return max;
}

// findmin function
int LinkedList::findmin() const
{
    int min = -1;
    BEGIN_TRANSACTION {
        rd_ptr<LLNode> curr(sentinel);
        curr = curr->get_next(curr);
        if (curr != NULL)
            min = curr->get_val(curr);
    } END_TRANSACTION;
    return min;
}

// write to the head of the list
void LinkedList::touch_head()
{
    BEGIN_TRANSACTION {
        wr_ptr<LLNode> curr(sentinel);
        curr->set_next(curr->get_next(curr), curr);
    } END_TRANSACTION;
}

// remove a node if its value == val
void LinkedList::remove(int val)
{
    BEGIN_TRANSACTION {
        if (!bench::early_tx_terminate) {
        // find the node whose val matches the request
        rd_ptr<LLNode> prev(sentinel);
        rd_ptr<LLNode> curr(prev->get_next(prev));
        while (curr != NULL) {
            // if we find the node, disconnect it and end the search
            if (curr->get_val(curr) == val) {
                wr_ptr<LLNode> mod_point(prev);
                mod_point->set_next(curr->get_next(curr), mod_point);

                // delete curr...
                tx_delete(curr);
                break;
            }
            else if (curr->get_val(curr) > val) {
                // this means the search failed
                break;
            }
            prev = curr;
            curr = prev->get_next(prev);
        }
        }
    } END_TRANSACTION;
}


// print the list
void LinkedList::print() const
{
    BEGIN_TRANSACTION {
        rd_ptr<LLNode> curr(sentinel);
        curr = curr->get_next(curr);
        std::cout << "list :: ";
        while (curr != NULL) {
            std::cout << curr->get_val(curr) << "->";
            curr = curr->get_next(curr);
        }
        std::cout << "NULL" << std::endl;
    } END_TRANSACTION;
}

// search function
void LinkedList::overwrite(int val)
{
    BEGIN_TRANSACTION {
        if (!bench::early_tx_terminate) {
            rd_ptr<LLNode> curr(sentinel);
            curr = curr->get_next(curr);

            while (curr != NULL) {
                if (curr->get_val(curr) >= val)
                    break;
                wr_ptr<LLNode> wcurr(curr);
                wcurr->set_val(wcurr->get_val(wcurr), wcurr);
                curr = wcurr->get_next(wcurr);
            }
        }
    } END_TRANSACTION;
}
