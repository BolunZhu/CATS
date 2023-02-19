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

#include "RBTreeLarge.hpp"
#include <stdio.h>
#include <climits>

using namespace stm;
using namespace bench;

// basic constructor
RBNodeLarge::RBNodeLarge()
    : m_color(BLACK),
      m_val(-1),
      m_parent(NULL)
{
    // fill in the padding
    for (int i = 0; i < 1024; i++)
        pad[i] = i;
}

// helper function:  set the child of node_rw from within a transaction
void setChild(const wr_ptr<RBNodeLarge>& node_rw, int cID,
              const sh_ptr<RBNodeLarge>& c)
{
    node_rw->set_child(cID, c, node_rw);

    if (c != NULL)
    {
        // update c's parent pointer
        wr_ptr<RBNodeLarge> c_rw(c);
        c_rw->set_parent(node_rw, c_rw);
        c_rw->set_ID(cID, c_rw);
    }
}

/* promote(4):

     2            4
    / \          / \
   1   4   =>   2   5
      / \      / \
     3   5    1   3
 */
// helper function:  promote a node
void promote(const wr_ptr<RBNodeLarge>& node_rw)
{
    wr_ptr<RBNodeLarge> parent_rw(node_rw->get_parent(node_rw));
    wr_ptr<RBNodeLarge> grandparent_rw(parent_rw->get_parent(parent_rw));
    int nodeID = node_rw->get_ID(node_rw);
    int parentID = parent_rw->get_ID(parent_rw);
    setChild(parent_rw, nodeID, node_rw->get_child(1 - nodeID, node_rw));
    setChild(node_rw, 1 - nodeID, parent_rw);
    setChild(grandparent_rw, parentID, node_rw);
}

// binary search for the node that has v as its value
bool RBTreeLarge::lookup(int v) const
{
    bool found = false;

    BEGIN_TRANSACTION;

    found = false;
    // find v
    rd_ptr<RBNodeLarge> sentinel_r(sentinel);
    sh_ptr<RBNodeLarge> x = sentinel_r->get_child(0, sentinel_r);
    rd_ptr<RBNodeLarge> x_r;

    while (x != NULL)
    {
        x_r = x;

        if (x_r->get_val(x_r) == v)
        {
            found = true;
            break;
        }
        else
        {
            x = x_r->get_child(v < x_r->get_val(x_r) ? 0 : 1, x_r);
        }
    }

    END_TRANSACTION;

    return found;
}

// insert a node with v as its value if no such node exists in the tree
void RBTreeLarge::insert(int v)
{
    BEGIN_TRANSACTION;

    sh_ptr<RBNodeLarge> cx;

    bool found = false;         // found means v is in the tree already
    // find insertion point
    rd_ptr<RBNodeLarge> sentinel_r(sentinel);
    rd_ptr<RBNodeLarge> x_r = sentinel_r;
    int cID = 0;
    rd_ptr<RBNodeLarge> cx_r;

    while (x_r->get_child(cID, x_r) != NULL)
    {
        cx_r = x_r->get_child(cID, x_r);

        if (cx_r->get_val(cx_r) == v)
        {
            found = true;
            break; // don't add existing key
        }

        cID = v < cx_r->get_val(cx_r) ? 0 : 1;
        x_r = cx_r;
    }

    // if the element isn't in the tree, add it and balance the tree
    if (!found)
    {
        wr_ptr<RBNodeLarge> x_rw(x_r);
        cx = sh_ptr<RBNodeLarge>(new RBNodeLarge());
        wr_ptr<RBNodeLarge> cx_rw(cx);
        cx_rw->set_color(RED, cx_rw);
        cx_rw->set_val(v, cx_rw);
        setChild(x_rw, cID, cx);

        wr_ptr<RBNodeLarge> px_rw;
        wr_ptr<RBNodeLarge> y_rw;
        // balance the tree
        while (true)
        {
            x_rw = cx_rw->get_parent(cx_rw);

            if ((x_rw->get_parent(x_rw) == sentinel) ||
                (BLACK == x_rw->get_color(x_rw)))
                break;

            px_rw = x_rw->get_parent(x_rw);
            y_rw = px_rw->get_child(1-x_rw->get_ID(x_rw), px_rw);

            if ((y_rw != NULL) && (RED == y_rw->get_color(y_rw)))
            {
                x_rw->set_color(BLACK, x_rw);
                y_rw->set_color(BLACK, y_rw);
                px_rw->set_color(RED, px_rw);
                cx_rw = px_rw;
            }
            else
            {
                if (cx_rw->get_ID(cx_rw) != x_rw->get_ID(x_rw))
                {
                    promote(cx_rw);
                    wr_ptr<RBNodeLarge> t = cx_rw;
                    cx_rw = x_rw;
                    x_rw = t;
                }

                x_rw->set_color(BLACK, x_rw);
                px_rw->set_color(RED, px_rw);
                promote(x_rw);
            }
        }

        sentinel_r = sentinel;

        sh_ptr<RBNodeLarge> root = sentinel_r->get_child(0, sentinel_r);

        if (root != NULL)
        {
            rd_ptr<RBNodeLarge> root_r(root);

            if (root_r->get_color(root_r) != BLACK)
            {
                wr_ptr<RBNodeLarge> root_rw(root_r);
                root_rw->set_color(BLACK, root_rw);
            }
        }
    }

    END_TRANSACTION;
}

// remove the node with v as its value if it exists in the tree
void RBTreeLarge::remove(int v)
{
    BEGIN_TRANSACTION;

    // find v
    rd_ptr<RBNodeLarge> sentinel_r(sentinel);
    sh_ptr<RBNodeLarge> x = sentinel_r->get_child(0, sentinel_r);
    rd_ptr<RBNodeLarge> x_r;

    while (x != NULL)
    {
        x_r = x;

        if (x_r->get_val(x_r) == v)
            break;
        else
            x = x_r->get_child(v < x_r->get_val(x_r) ? 0 : 1, x_r);
    }

    // if we found v, remove it
    if (x != NULL)
    {
        wr_ptr<RBNodeLarge> x_rw(x);

        // ensure that we are deleting a node with at most one child
        if ((x_rw->get_child(0, x_rw) != NULL) &&
            (x_rw->get_child(1, x_rw) != NULL))
        {
            rd_ptr<RBNodeLarge> y_r(x_rw->get_child(1, x_rw));

            while (y_r->get_child(0, y_r) != NULL)
                y_r = y_r->get_child(0, y_r);

            x_rw->set_val(y_r->get_val(y_r), x_rw);
            x = y_r;
            x_rw = x;
        }

        // perform deletion
        sh_ptr<RBNodeLarge> p = x_rw->get_parent(x_rw);
        wr_ptr<RBNodeLarge> p_rw(p);
        int cID = (x_rw->get_child(0, x_rw) != NULL) ? 0 : 1;
        sh_ptr<RBNodeLarge> c = x_rw->get_child(cID, x_rw);
        setChild(p_rw, x_rw->get_ID(x_rw), c);

        // fix black height violations
        if ((BLACK == x_rw->get_color(x_rw)) && (c != NULL))
        {
            rd_ptr<RBNodeLarge> c_r(c);

            if (RED == c_r->get_color(c_r))
            {
                wr_ptr<RBNodeLarge> c_rw(c_r);
                x_rw->set_color(RED, x_rw);
                c_rw->set_color(BLACK, c_rw);
            }
        }

        wr_ptr<RBNodeLarge> y_rw = x_rw;
        wr_ptr<RBNodeLarge> s_rw;

        while ((y_rw->get_parent(y_rw) != sentinel) &&
               (BLACK == y_rw->get_color(y_rw)))
        {
            p = y_rw->get_parent(y_rw);
            p_rw = p;
            // s_rw is a sibling
            s_rw = p_rw->get_child(1 - y_rw->get_ID(y_rw), p_rw);

            /* we'd like y's sibling s to be black
             * if it's not, promote it and recolor
             */
            if (RED == s_rw->get_color(s_rw))
            {
                /*
                    Bp          Bs
                    / \         / \
                  By  Rs  =>  Rp  B2
                      / \     / \
                     B1 B2  By  B1
                 */
                p_rw->set_color(RED, p_rw);
                s_rw->set_color(BLACK, s_rw);
                promote(s_rw);
                // reset s
                s_rw = p_rw->get_child(1 - y_rw->get_ID(y_rw), p_rw);
            }

            sh_ptr<RBNodeLarge> n =
                s_rw->get_child(1 - y_rw->get_ID(y_rw), s_rw);
            rd_ptr<RBNodeLarge> n_r;

            if (n != NULL)
            {
                n_r = n;

                if (RED == n_r->get_color(n_r))
                {
                  farNephew:
                    wr_ptr<RBNodeLarge> n_rw(n);
                    /*
                        ?p          ?s
                        / \         / \
                      By  Bs  =>  Bp  Bn
                          / \         / \
                         ?1 Rn      By  ?1
                    */
                    s_rw->set_color(p_rw->get_color(p_rw), s_rw);
                    p_rw->set_color(BLACK, p_rw);
                    n_rw->set_color(BLACK, n_rw);
                    promote(s_rw);
                    break; // problem solved
                }
                else
                {
                    goto blackn1;
                }
            }
            else
            {
              blackn1:
                n = s_rw->get_child(y_rw->get_ID(y_rw), s_rw);

                if (n != NULL)
                {
                    n_r = n;
                    if (RED == n_r->get_color(n_r))
                    {
                        wr_ptr<RBNodeLarge> n_rw(n_r);
                        /*
                            ?p          ?p
                            / \         / \
                          By  Bs  =>  By  Bn
                              / \           \
                             Rn B1          Rs
                                      \
                                      B1
                        */
                        s_rw->set_color(RED, s_rw);
                        n_rw->set_color(BLACK, n_rw);
                        wr_ptr<RBNodeLarge> t = s_rw;
                        promote(n_rw);
                        s_rw = n_rw;
                        n = t;
                        goto farNephew; // now the far nephew is red
                    }
                    else
                    {
                        goto blackn2;
                    }
                }
                else
                {
                  blackn2:
                    /*
                        ?p          ?p
                        / \         / \
                      Bx  Bs  =>  Bp  Rs
                          / \         / \
                         B1 B2      B1  B2
                    */
                    // propagate the problem upwards
                    s_rw->set_color(RED, s_rw);
                }
            }

            // advance to parent and balance again
            y_rw = p_rw;
        }

        // if y was red, this fixes the balance
        y_rw->set_color(BLACK, y_rw);

        // free storage associated with deleted node
        wr_ptr<RBNodeLarge> param1(x_rw->get_parent(x_rw));
        setChild(param1, x_rw->get_ID(x_rw), c);
        // children should NOT be freed
        x_rw->set_child(0, sh_ptr<RBNodeLarge>(NULL), x_rw);
        x_rw->set_child(1, sh_ptr<RBNodeLarge>(NULL), x_rw);

        tx_delete(x);
    }

    END_TRANSACTION;
}

// returns black-height when balanced and -1 otherwise
static int blackHeight(const sh_ptr<RBNodeLarge>& x)
{
    if (!x)
    {
        return 0;
    }
    else
    {
        const rd_ptr<RBNodeLarge> x_r(x);
        int bh0 = blackHeight(x_r->get_child(0, x_r));
        int bh1 = blackHeight(x_r->get_child(1, x_r));
        if ((bh0 >= 0) && (bh1 == bh0))
            return BLACK==x_r->get_color(x_r) ? 1+bh0 : bh0;
        else
            return -1;
    }
}

// returns true when a red node has a red child
static bool redViolation(const rd_ptr<RBNodeLarge>& p_r,
                         const sh_ptr<RBNodeLarge>& x)
{
    if (!x) {
        return false;
    }
    else {
        const rd_ptr<RBNodeLarge> x_r(x);
        return ((RED == p_r->get_color(p_r) &&
                 RED == x_r->get_color(x_r)) ||
                (redViolation(x_r, x_r->get_child(0, x_r))) ||
                (redViolation(x_r, x_r->get_child(1, x_r))));
    }
}

// returns true when all nodes' parent fields point to their parents
static bool validParents(const sh_ptr<RBNodeLarge>& p, int xID,
                         const sh_ptr<RBNodeLarge>& x)
{
    if (!x) {
        return true;
    }
    else {
        const rd_ptr<RBNodeLarge> x_r(x);
        return ((x_r->get_parent(x_r) == p) &&
                (x_r->get_ID(x_r) == xID) &&
                (validParents(x, 0, x_r->get_child(0, x_r))) &&
                (validParents(x, 1, x_r->get_child(1, x_r))));
    }
}

// returns true when the tree is ordered
static bool inOrder(const sh_ptr<RBNodeLarge>& x,
                    int lowerBound, int upperBound)
{
    if (!x)
        return true;
    else {
        const rd_ptr<RBNodeLarge> x_r(x);
        return ((lowerBound <= x_r->get_val(x_r)) &&
                (x_r->get_val(x_r) <= upperBound) &&
                (inOrder(x_r->get_child(0, x_r), lowerBound,
                         x_r->get_val(x_r) - 1)) &&
                (inOrder(x_r->get_child(1, x_r),
                         x_r->get_val(x_r) + 1, upperBound)));
    }
}

// build an empty tree
RBTreeLarge::RBTreeLarge()
    : sentinel(new RBNodeLarge())
{ }

// sanity check of the RBTree data structure
bool RBTreeLarge::isSane() const
{
    bool sane = false;

    BEGIN_TRANSACTION;

    sane = false;
    const rd_ptr<RBNodeLarge> sentinel_r(sentinel);
    sh_ptr<RBNodeLarge> root = sentinel_r->get_child(0, sentinel_r);

    if (!root)
    {
        sane = true; // empty tree needs no checks
    }
    else
    {
        const rd_ptr<RBNodeLarge> root_r(root);
        sane = ((BLACK == root_r->get_color(root_r)) &&
                (blackHeight(root) >= 0) &&
                !(redViolation(sentinel_r, root)) &&
                (validParents(sentinel, 0, root)) &&
                (inOrder(root, INT_MIN, INT_MAX)));
    }

    END_TRANSACTION;

    return sane;
}

// print a node and recurse on its children
static void printNode(const sh_ptr<RBNodeLarge>& x, int indent = 0)
{
    if (!x)
        return;

    const rd_ptr<RBNodeLarge> x_r(x);

    printNode(x_r->get_child(0, x_r), indent + 2);

    for (int i = 0; i < indent; i++)
        putchar('-');

    printf("%c%d\n",
           RED==x_r->get_color(x_r) ? 'R' : 'B',
           x_r->get_val(x_r));
    printNode(x_r->get_child(1, x_r), indent + 2);

    if (!indent)
        printf("\n\n");
}

// "transaction" that prints a tree; only run this in single-thread mode!
void RBTreeLarge::print(int indent) const
{
    BEGIN_TRANSACTION;

    const rd_ptr<RBNodeLarge> sentinel_r(sentinel);
    printNode(sentinel_r->get_child(0, sentinel_r));

    END_TRANSACTION;
}
