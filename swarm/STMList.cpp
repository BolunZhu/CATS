/******************************************************************************
 *
 * Copyright (c) 2007, 2008, 2009
 * University of Rochester
 * Department of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the name of the University of Rochester nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "STMList.hpp"

void STMList::Insert(wr_ptr<STMList> wrThis, sh_ptr<GameObject> val,
                     int iThreadID)
{
    // traverse the list to find the insertion point
    rd_ptr<STMListNode> prev(wrThis->get_sentinel(wrThis));
    rd_ptr<STMListNode> curr(prev->get_next(prev));
    while (curr != NULL) {
        sh_ptr<GameObject> v = curr->get_val(curr);
        if ((v == val) || (v > val))
            break;
        prev = curr;
        curr = prev->get_next(prev);
    }

    // now insert new_node between prev and curr
    if (!curr || curr->get_val(curr) != val) {
        wr_ptr<STMListNode> insert_point(prev);
        insert_point->set_next(sh_ptr<STMListNode>(new STMListNode(val, curr)),
                               insert_point);
    }
}

void STMList::Remove(wr_ptr<STMList> wrThis, sh_ptr<GameObject> val,
                     int iThreadID)
{
    // find the node whose val matches the request
    rd_ptr<STMListNode> prev(wrThis->get_sentinel(wrThis));
    rd_ptr<STMListNode> curr(prev->get_next(prev));
    while (curr != NULL) {
        sh_ptr<GameObject> v = curr->get_val(curr);
        // if we find the node, disconnect and free it
        if (v == val) {
            wr_ptr<STMListNode> mod_point(prev);
            mod_point->set_next(curr->get_next(curr), mod_point);
            tx_delete(curr);
            break;
        }
        else if (v > val) {
            // this means the search failed
            break;
        }
        prev = curr;
        curr = prev->get_next(prev);
    }
}

std::vector< sh_ptr<GameObject> >
STMList::GrabAll(rd_ptr<STMList> rdThis, int iThreadID) const
{
    std::vector< sh_ptr<GameObject> > returnVec;

    rd_ptr<STMListNode> curr(rdThis->get_sentinel(rdThis));
    curr = curr->get_next(curr);

    if (curr != NULL) {
        while (curr != NULL) {
            returnVec.push_back(curr->get_val(curr));
            curr = curr->get_next(curr);
        }
    }

    return returnVec;
}
