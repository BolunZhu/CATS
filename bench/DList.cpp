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

#include "DList.hpp"

using namespace stm;
using namespace bench;

// constructor: head and tail have extreme values, point to each other
DList::DList(int max) : head(new DNode(-1)), tail(new DNode(max))
{
    un_ptr<DNode> h(head);
    un_ptr<DNode> t(tail);

    h->set_next(t, h);
    t->set_prev(h, t);
}

// simple sanity check:  make sure all elements of the list are in sorted order
bool DList::isSane(void) const
{
    bool sane = false;

    BEGIN_TRANSACTION {
        sane = true;

        // forward traversal
        rd_ptr<DNode> prev(head);
        rd_ptr<DNode> curr(prev->get_next(prev));
        while (curr != NULL) {
            // ensure sorted order
            if (prev->get_val(prev) >= curr->get_val(curr)) {
                sane = false;
                break;
            }
            // ensure curr->prev->next == curr
            rd_ptr<DNode> p (curr->get_prev(curr));
            if (p->get_next(p) != curr) {
                sane = false;
                break;
            }

            prev = curr;
            curr = curr->get_next(curr);
        }

        // backward traversal
        prev = tail;
        curr = prev->get_prev(prev);
        while (curr != NULL) {
            // ensure sorted order
            if (prev->get_val(prev) < curr->get_val(curr)) {
                sane = false;
                break;
            }
            // ensure curr->next->prev == curr
            rd_ptr<DNode> n (curr->get_next(curr));
            if (n->get_prev(n) != curr) {
                sane = false;
                break;
            }

            prev = curr;
            curr = curr->get_prev(curr);
        }

    } END_TRANSACTION;

    return sane;
}

// insert method; find the right place in the list, add val so that it is in
// sorted order; if val is already in the list, exit without inserting
void DList::insert(int val)
{
    BEGIN_TRANSACTION {
        // traverse the list to find the insertion point
        rd_ptr<DNode> prev(head);
        rd_ptr<DNode> curr(prev->get_next(prev));

        while (curr != NULL) {
            if (curr->get_val(curr) >= val)
                break;

            prev = curr;
            curr = prev->get_next(prev);
        }

        // now insert new_node between prev and curr
        if (curr->get_val(curr) > val) {
            wr_ptr<DNode> before(prev);
            wr_ptr<DNode> after(curr);
            sh_ptr<DNode> between(new DNode(val, before, after));
            before->set_next(between, before);
            after->set_prev(between, after);
        }

    } END_TRANSACTION;
}

// search for a value
bool DList::lookup(int val) const
{
    bool found = false;

    BEGIN_TRANSACTION {
        rd_ptr<DNode> curr(head);
        curr = curr->get_next(curr);
        while (curr != NULL) {
            if (curr->get_val(curr) >= val)
                break;
            curr = curr->get_next(curr);
        }
        found = ((curr != NULL) && (curr->get_val(curr) == val));
    } END_TRANSACTION;

    return found;
}

// remove a node if its value == val
void DList::remove(int val)
{
    BEGIN_TRANSACTION {
        // find the node whose val matches the request
        rd_ptr<DNode> prev(head);
        rd_ptr<DNode> curr(prev->get_next(prev));

        while (curr != NULL) {
            // if we find the node, disconnect it and end the search
            if (curr->get_val(curr) == val) {
                wr_ptr<DNode> before(prev);
                wr_ptr<DNode> after(curr->get_next(curr));
                before->set_next(after, before);
                after->set_prev(before, after);

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

    } END_TRANSACTION;
}

// print the list
void DList::print() const
{
    BEGIN_TRANSACTION {
        rd_ptr<DNode> curr(head);
        //curr = curr->get_next(curr);

        std::cout << "list :: ";

        while (curr != NULL) {
            std::cout << curr->get_val(curr) << "->";
            curr = curr->get_next(curr);
        }

        std::cout << "NULL" << std::endl;
    } END_TRANSACTION;
}

void DList::increment_forward()
{
    BEGIN_TRANSACTION {
        // forward traversal
        rd_ptr<DNode> prev(head);
        wr_ptr<DNode> curr(prev->get_next(prev));
        while (curr != tail) {
            // increment curr
            curr->set_val(1 + curr->get_val(curr), curr);
            curr = curr->get_next(curr);
        }
    } END_TRANSACTION;
}

void DList::increment_backward()
{
    BEGIN_TRANSACTION {
        // backward traversal
        rd_ptr<DNode> prev(tail);
        wr_ptr<DNode> curr(prev->get_prev(prev));
        while (curr != head) {
            // increment curr
            curr->set_val(1 + curr->get_val(curr), curr);
            curr = curr->get_prev(curr);
        }
    } END_TRANSACTION;
}

// increment every seqth element, starting with start, moving forward
void DList::increment_forward_pattern(int start, int seq)
{
    BEGIN_TRANSACTION {
        int sum = 0;
        // forward traversal to element # start
        rd_ptr<DNode> prev(head);
        rd_ptr<DNode> curr(prev->get_next(prev));
        for (int i = 0; i < start; i++) {
            curr = curr->get_next(curr);
        }
        // now do the remainder of the traversal, incrementing every seqth
        // element
        int ctr = seq;
        while (curr != tail) {
            // increment the seqth element
            if (ctr == seq) {
                ctr = 0;
                wr_ptr<DNode> cw(curr);
                cw->set_val(1 + cw->get_val(cw), cw);
                curr = cw;
            }
            ctr++;
            sum += curr->get_val(curr);
            curr = curr->get_next(curr);
        }
    } END_TRANSACTION;
}

// increment every element, starting with start, moving backward
void DList::increment_backward_pattern(int start, int seq)
{
    BEGIN_TRANSACTION {
        int sum = 0;
        // backward traversal to element # start
        rd_ptr<DNode> prev(tail);
        rd_ptr<DNode> curr(prev->get_prev(prev));
        for (int i = 0; i < start; i++) {
            curr = curr->get_prev(curr);
        }
        // now do the remainder of the traversal, incrementing every seqth
        // element
        int ctr = seq;
        while (curr != head) {
            // increment the seqth element
            if (ctr == seq) {
                ctr = 0;
                wr_ptr<DNode> cw(curr);
                cw->set_val(1 + cw->get_val(cw), cw);
                curr = cw;
            }
            ctr++;
            sum += curr->get_val(curr);
            curr = curr->get_prev(curr);
        }
    } END_TRANSACTION;
}

// increment every seqth element, starting with start, moving forward
void DList::increment_chunk(int chunk_num, int chunk_size)
{
    int startpoint = chunk_num * chunk_size;

    BEGIN_TRANSACTION {
        int sum = 0;
        sh_ptr<DNode> chunk_start(NULL);
        int ctr = 0;

        // forward traversal to compute sum and to find chunk_start
        rd_ptr<DNode> prev(head);
        rd_ptr<DNode> curr(prev->get_next(prev));
        while (curr != tail) {
            // if this is the start of our chunk, save the pointer
            if (ctr++ == startpoint)
                chunk_start = curr;
            // add this value to the sum
            sum += curr->get_val(curr);
            // move to next node
            curr = curr->get_next(curr);
        }
        // OK, at this point we should have the ID of the chunk we're going to
        // work on.  increment everything in our chunk
        if (chunk_start != NULL) {
            // avoid TLS overhead on every loop iteration:
            wr_ptr<DNode> wr(chunk_start);
            // increment /chunk_size/ elements
            for (int i = 0; i < chunk_size; i++) {
                // don't increment if we reach the tail
                if (chunk_start == tail)
                    break;
                // increment, move to next
                wr->set_val(1 + wr->get_val(wr), wr);
                wr = wr->get_next(wr);
            }
        }

    } END_TRANSACTION;
}
