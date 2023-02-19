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

#ifndef PRIV_LIST_HPP__
#define PRIV_LIST_HPP__

#include <stm/stm.hpp>
#include <iostream>

namespace bench
{
    // PrivNode is a single node in a sorted linked list
    class PrivNode : public stm::Object
    {
        GENERATE_FIELD(int, val);
        GENERATE_FIELD(stm::sh_ptr<PrivNode>, next);

      public:

        PrivNode(int val = -1) : m_val(val), m_next() { }

        PrivNode(int val, const stm::sh_ptr<PrivNode>& next)
            : m_val(val), m_next(next) { }

    };

    // Set of PrivNodes represented as a linked list in sorted order
    class PrivList : public Benchmark
    {
        stm::sh_ptr<PrivNode> sentinel;
        int m_max;

      public:
        // constructor just makes a sentinel for the data structure
        PrivList(int _max) : sentinel(new PrivNode()), m_max(_max) { }

        virtual void random_transaction(thread_args_t* args,
                                        unsigned int*  seed,
                                        unsigned int   val,
                                        int            chance)
        {
            if (insertNextOrClearAll())
                ++args->count[TXN_INSERT];
            else
                ++args->count[TXN_REMOVE];
        }

        // make sure the list is in sorted order
        virtual bool sanity_check() const;

        // no data structure verification is implemented for the PrivList yet
        virtual bool verify(VerifyLevel_t v) { return true; }

        /**
         *  There is only one operation on the PrivList: insertNextOrClearAll.
         *  This operation attempts to add the next integer between 0 and m_max
         *  to the sorted list.  If it finds that m_max has already been
         *  inserted, the operation privatizes the whole list and deletes all
         *  elements.
         */
        bool insertNextOrClearAll();
    };
} // namespace bench


// simple sanity check:  make sure all elements of the list are in sorted order
bool bench::PrivList::sanity_check(void) const
{
    bool sane = false;

    BEGIN_TRANSACTION;

    sane = true;
    stm::rd_ptr<PrivNode> prev(sentinel);
    stm::rd_ptr<PrivNode> curr(prev->get_next(prev));

    while (curr != NULL) {
        if (prev->get_val(prev) >= curr->get_val(curr)) {
            sane = false;
            break;
        }

        prev = curr;
        curr = curr->get_next(curr);
    }

    END_TRANSACTION;

    return sane;
}

/**
 * If the max element is < m_max, insert the next integer into the end of the
 * list.  Otherwise, clear the whole list via privatization.
 */
bool bench::PrivList::insertNextOrClearAll()
{
    // if we privatize the list, it will go into here
    stm::sh_ptr<PrivNode> priv(NULL);

    BEGIN_TRANSACTION;

    // reset locals
    priv = stm::sh_ptr<PrivNode>(NULL);

    // step one: traverse the list to find the max, and store the last node as
    // prev
    int max = -1;
    stm::rd_ptr<PrivNode> prev(sentinel);
    stm::rd_ptr<PrivNode> curr(prev->get_next(prev));
    while (curr != NULL) {
        max = curr->get_val(curr);
        prev = curr;
        curr = prev->get_next(prev);
    }

    // step two: either insert a new element or excise the whole tree
    if (max < m_max) {
        // insert a new node
        stm::wr_ptr<PrivNode> insert_pt(prev);
        insert_pt->set_next(stm::sh_ptr<PrivNode>(new PrivNode(max+1, curr)),
                           insert_pt);
    }
    else {
        // excise
        // first go back to the start of the tree
        prev = sentinel;
        // now 'leak' the first non-sentinel node into priv
        priv = prev->get_next(prev);
        // now write a NULL in sentinel->next
        stm::wr_ptr<PrivNode> excise_pt(prev);
        excise_pt->set_next(stm::sh_ptr<PrivNode>(NULL), excise_pt);
    }

    END_TRANSACTION;

    // if we've got a priv, then we need to use it:
    if (priv != NULL) {
        // wait for all transactions to finish
        stm::fence();

        // now start using un_ptrs:
        stm::un_ptr<PrivNode> prev(priv);
        stm::un_ptr<PrivNode> curr(prev->get_next(prev));

        while (curr != NULL) {
            tx_delete(prev);

            prev = curr;
            curr = curr->get_next(curr);
        }

        tx_delete(prev);

        // return false: we were a privatizing tx
        return false;
    }

    // return true: we were an inserting tx
    return true;
}

#endif // PRIV_LIST_HPP__
