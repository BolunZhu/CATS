///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006, 2007, 2008, 2009
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

/*  tm_list_set.hpp
 *
 *  Transactional list-based set.
 *  Element type must be the same size as unsigned long.
 */

#ifndef TM_LIST_SET_HPP__
#define TM_LIST_SET_HPP__

#include <cassert>
#include "config.hpp"
#include "tm_set.hpp"

template<typename T>
class tm_list_set;

// LLNode is a single node in a sorted linked list
//
template<class T>
class LLNode : public stm::Object
{
    friend class tm_list_set<T>;   // all fields are nominally private

    GENERATE_FIELD(T, val);
    GENERATE_FIELD(stm::sh_ptr<LLNode<T> >, next_node);

    // constructors
    //
    LLNode() : m_val(), m_next_node(NULL) { }
    LLNode(T val, const stm::sh_ptr<LLNode<T> >& next)
        : m_val(val), m_next_node(next) { }

    // clone method for use in RSTM
    virtual LLNode* clone() const
    {
        T val = m_val;
        stm::sh_ptr<LLNode<T> > next = m_next_node;
        return new LLNode(val, next);
    }

#ifdef NEED_REDO_METHOD
    virtual void redo(SharedBase* l_sh)
    {
        LLNode* l = static_cast<LLNode*>(l_sh);
        m_val = l->m_val;
        m_next_node = l->m_next_node;
    }
#endif
};

template<typename T>
class tm_list_set : public tm_set<T>
{
    stm::sh_ptr<LLNode<T> > head_node;

    // no implementation; forbids passing by value
    tm_list_set(const tm_list_set&);
    // no implementation; forbids copying
    tm_list_set& operator=(const tm_list_set&);

    // Share code among transactional and nontransactional
    // implementations of methods.
    //
    template<class rdp, class wrp> void insert(const T item);
    template<class rdp, class wrp> void remove(const T item);
    template<class rdp> bool lookup(const T item);

  public:
    // note that assert in constructor guarantees that items and
    // unsigned ints are the same size
    //
    virtual void tx_insert(const T item)
    {
        insert<stm::rd_ptr< LLNode<T> >, stm::wr_ptr< LLNode<T> > >(item);
    }

    virtual void tx_remove(const T item)
    {
        remove<stm::rd_ptr< LLNode<T> >, stm::wr_ptr< LLNode<T> > >(item);
    }

    virtual bool tx_lookup(const T item)
    {
        return lookup<stm::rd_ptr< LLNode<T> > >(item);
    }

    virtual void pv_insert(const T item)
    {
        insert<stm::un_ptr< LLNode<T> >, stm::un_ptr< LLNode<T> > >(item);
    }

    virtual void pv_remove(const T item)
    {
        remove<stm::un_ptr<LLNode<T> >, stm::un_ptr<LLNode<T> > >(item);
    }

    virtual bool pv_lookup(const T item)
    {
        return lookup<stm::un_ptr<LLNode<T> > >(item);
    }

    virtual void pv_apply_to_all(void (*f)(T item));

    tm_list_set() : head_node(new LLNode<T>())
    {
        assert(sizeof(T) == sizeof(unsigned long));
    }

    // Destruction not currently supported.
    virtual ~tm_list_set() { assert(false); }

    int pv_size() const;
};

// insert into the list, unless the item is already in the list
template<typename T>
template<class rdp, class wrp>
void tm_list_set<T>::insert(const T item)
{
    // traverse the list to find the insertion point
    rdp prev(head_node);
    rdp curr(prev->get_next_node(prev));
    while (curr != 0) {
        if (curr->get_val(curr) == item)
            return;
        prev = curr;
        curr = prev->get_next_node(prev);
    }

    wrp insert_point(prev);      // upgrade to writable
    insert_point->set_next_node(stm::sh_ptr< LLNode<T> >(new LLNode<T>(item,
                                                                       curr)),
                                insert_point);
}


// remove a node if its value == item
template<typename T>
template<class rdp, class wrp>
void tm_list_set<T>::remove(const T item)
{
    // find the node whose val matches the request
    rdp prev(head_node);
    rdp curr(prev->get_next_node(prev));
    while (curr != 0) {
        // if we find the node, disconnect it and end the search
        if (curr->get_val(curr) == item) {
            wrp mod_point(prev);     // upgrade to writable
            mod_point->set_next_node(curr->get_next_node(curr), mod_point);
            //            tx_delete(curr);
            break;
        }
        prev = curr;
        curr = prev->get_next_node(prev);
    }
}

// find out whether item is in list
template<typename T>
template<class rdp>
bool tm_list_set<T>::lookup(const T item)
{
    rdp curr(head_node);
    curr = curr->get_next_node(curr);
    while (curr != 0) {
        if (curr->get_val(curr) == item)
            return true;
        curr = curr->get_next_node(curr);
    }
    return false;
}

// apply a function to every element of the list
template<typename T>
void tm_list_set<T>::pv_apply_to_all(void (*f)(T item))
{
    stm::un_ptr<LLNode<T> > curr(head_node);
    curr = curr->get_next_node(curr);
    while (curr != 0) {
        f(curr->get_val(curr));
        curr = curr->get_next_node(curr);
    }
}

// count elements in the list
template<typename T>
int tm_list_set<T>::pv_size() const
{
    int rtn = -1;
    stm::un_ptr<LLNode<T> > curr(head_node);
    while (curr != 0) {
        rtn++;
        curr = curr->get_next_node(curr);
    }
    return rtn;
}

#endif // TM_LIST_SET_HPP__
