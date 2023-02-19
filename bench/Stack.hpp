///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2008, 2009
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

#ifndef STACK_HPP__
#define STACK_HPP__

/*** a very simple stack implementation */
template <typename T>
class TxStack
{
    /**
     * the stack is implemented as a list, so nodes are dynamically allocated
     */
    struct StackNode : public stm::Object
    {
        // the item in this position
        GENERATE_FIELD(T, val);
        // the next thing in the stack
        GENERATE_FIELD(stm::sh_ptr<StackNode>, next);

        StackNode(T v, stm::sh_ptr<StackNode> n) : m_val(v), m_next(n) { }
        StackNode() : m_val(), m_next(NULL) { }
    };

    /*** sentinel so we never have a NULL stack */
    stm::sh_ptr<StackNode> sentinel;

  public:

    TxStack() : sentinel(new StackNode()) { }

    /*** transactionally push onto the stack */
    void push_tx(T i)
    {
        // open sentinel for reading
        stm::rd_ptr<StackNode> r_sent(sentinel);

        // create new stack node with next = sentinel->next
        stm::sh_ptr<StackNode> n(new StackNode(i, r_sent->get_next(r_sent)));

        // make sentinel point to this
        stm::wr_ptr<StackNode> w_sent(r_sent);
        w_sent->set_next(n, w_sent);
    }

    /*** transactionally pop from the stack */
    bool pop_tx(T& i)
    {
        // open sentinel, look at next
        stm::rd_ptr<StackNode> r_sent(sentinel);
        stm::rd_ptr<StackNode> r_next(r_sent->get_next(r_sent));
        if (r_next == NULL)
            return false;

        // next is not null, so read it in full
        stm::sh_ptr<StackNode> nextnext(r_next->get_next(r_next));
        i = r_next->get_val(r_next);
        // now disconnect next
        stm::wr_ptr<StackNode> w_sent(r_sent);
        w_sent->set_next(nextnext, w_sent);
        // delete next
        stm::tx_delete(r_next);

        // return true, since we set /i/
        return true;
    }
};

#endif // STACK_HPP__
