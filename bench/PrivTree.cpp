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

#include "PrivTree.hpp"
#include <stdio.h>

using namespace stm;
using namespace bench;

#ifndef INT_MIN
#define INT_MIN 0xFFFFFFFF
#define INT_MAX 0x7FFFFFFF
#endif

// binary search for the node that has v as its value
bool PrivTree::lookup(int v) const
{
    bool found = false;

    BEGIN_TRANSACTION {

        found = false;
        // find v
        rd_ptr<RBNode> sentinel_r(sentinel);
        sh_ptr<RBNode> x = sentinel_r->get_child(0, sentinel_r);
        rd_ptr<RBNode> x_r;

        while (x != NULL) {
            x_r = x;
            long xval = x_r->get_val(x_r);
            if (xval == v) {
                found = true;
                break;
            }
            else {
                x = x_r->get_child((v < xval) ? 0 : 1, x_r);
            }
        }

    } END_TRANSACTION;

    return found;
}

// insert a node with v as its value if no such node exists in the tree
void PrivTree::insert(int v)
{
    sh_ptr<RBNode> leaked_tree;
    bool leaked;

    BEGIN_TRANSACTION {
        bool found = false;         // found means v is in the tree already
        // find insertion point
        rd_ptr<RBNode> curr(sentinel);
        int cID = 0;
        rd_ptr<RBNode> child(curr->get_child(cID, curr));

        while (child != NULL) {
            long cval = child->get_val(child);
            if (cval == v) {
                found = true;
                break; // don't add existing key
            }
            cID = v < cval ? 0 : 1;
            curr = child;
            child = curr->get_child(cID, curr);
        }

        // if the element isn't in the tree, add it and balance the tree
        if (!found) {
            // create the new node ("child") and attach it as curr->child[cID]
            sh_ptr<RBNode> child(new RBNode(RED, v, curr, cID));
            wr_ptr<RBNode> curr_rw(curr);
            rd_ptr<RBNode> child_r(child);
            curr_rw->set_child(cID, child, curr_rw);

            // balance the tree
            while (true) {
                rd_ptr<RBNode> parent_r(child_r->get_parent(child_r));
                // read the parent of curr as gparent
                rd_ptr<RBNode> gparent_r(parent_r->get_parent(parent_r));

                if ((gparent_r == sentinel) ||
                    (BLACK == parent_r->get_color(parent_r)))
                    break;

                // cache the ID of the parent
                long pID = parent_r->get_ID(parent_r);
                // get parent's sibling as aunt
                rd_ptr<RBNode> aunt_r(gparent_r->get_child(1-pID, gparent_r));
                // gparent and parent will be written on all control paths
                wr_ptr<RBNode> gparent_w(gparent_r);
                wr_ptr<RBNode> parent_w(parent_r);

                if ((aunt_r != NULL) && (RED == aunt_r->get_color(aunt_r))) {
                    // set parent and aunt to BLACK, grandparent to RED
                    parent_w->set_color(BLACK, parent_w);
                    wr_ptr<RBNode> aunt_rw(aunt_r);
                    aunt_rw->set_color(BLACK, aunt_rw);
                    gparent_w->set_color(RED, gparent_w);
                    // now restart loop at gparent level
                    child_r = gparent_w;
                    continue;
                }

                long cID = child_r->get_ID(child_r);
                if (cID != pID) {
                    // promote child
                    wr_ptr<RBNode> child_rw(child_r);
                    sh_ptr<RBNode> baby(child_rw->get_child(1-cID, child_rw));
                    // set child's child to parent's cID'th child
                    parent_w->set_child(cID, baby, parent_w);
                    if (baby != NULL) {
                        wr_ptr<RBNode> baby_w(baby);
                        baby_w->set_parent(parent_w, baby_w);
                        baby_w->set_ID(cID, baby_w);
                    }
                    // move parent into baby's position as a child of child
                    child_rw->set_child(1-cID, parent_w, child_rw);
                    parent_w->set_parent(child_rw, parent_w);
                    parent_w->set_ID(1-cID, parent_w);
                    // move child into parent's spot as pID'th child of gparent
                    gparent_w->set_child(pID, child_rw, gparent_w);
                    child_rw->set_parent(gparent_w, child_rw);
                    child_rw->set_ID(pID, child_rw);
                    // promote(child_rw);
                    // now swap child with curr and continue
                    rd_ptr<RBNode> t(child_rw);
                    child_r = parent_w;
                    parent_w = t;
                }

                parent_w->set_color(BLACK, parent_w);
                gparent_w->set_color(RED, gparent_w);
                // promote parent
                wr_ptr<RBNode> ggparent_w(gparent_w->get_parent(gparent_w));
                int gID = gparent_w->get_ID(gparent_w);
                sh_ptr<RBNode> ochild = parent_w->get_child(1 - pID, parent_w);
                // make gparent's pIDth child ochild
                gparent_w->set_child(pID, ochild, gparent_w);
                if (ochild != NULL) {
                    wr_ptr<RBNode> ochild_w(ochild);
                    ochild_w->set_parent(gparent_w, ochild_w);
                    ochild_w->set_ID(pID, ochild_w);
                }
                // make gparent the 1-pID'th child of parent
                parent_w->set_child(1-pID, gparent_w, parent_w);
                gparent_w->set_parent(parent_w, gparent_w);
                gparent_w->set_ID(1-pID, gparent_w);
                // make parent the gIDth child of ggparent
                ggparent_w->set_child(gID, parent_w, ggparent_w);
                parent_w->set_parent(ggparent_w, parent_w);
                parent_w->set_ID(gID, parent_w);
                // promote(parent_w);
            }

            // now just set the root to black
            rd_ptr<RBNode> sentinel_r(sentinel);
            rd_ptr<RBNode> root_r(sentinel_r->get_child(0, sentinel_r));
            if (root_r->get_color(root_r) != BLACK) {
                wr_ptr<RBNode> root_rw(root_r);
                root_rw->set_color(BLACK, root_rw);
            }
        }
        // element 0 is the "privatize now" element.  If we just put it in,
        // then privatize the tree.
        if (v == 0) {
            // open the sentinel for writing
            wr_ptr<RBNode> w_s(sentinel);

            // copy the only child out of the sentinel into leaked_tree
            leaked_tree = w_s->get_child(0, w_s);
            leaked = true;

            // now manually reset the sentinel
            w_s->set_color(BLACK, w_s);
            w_s->set_val(-1, w_s);
            w_s->set_parent(sh_ptr<RBNode>(NULL), w_s);
            w_s->set_ID(0, w_s);
            w_s->set_child(0, sh_ptr<RBNode>(NULL), w_s);
            w_s->set_child(1, sh_ptr<RBNode>(NULL), w_s);
        }
        else {
            leaked = false;
        }
    } END_TRANSACTION;

    if (leaked) {
        // the transaction was an acquire
        acquire_fence();
        // now push the tree onto a stack for consumption by another thread
        BEGIN_TRANSACTION {
            STACK->push_tx(leaked_tree);
        } END_TRANSACTION;
    }
}

void PrivTree::consumer()
{
    // try to get something off of the stack
    sh_ptr<RBNode> private_tree;
    bool b = false;
    while (true) {
        BEGIN_TRANSACTION {
            b = STACK->pop_tx(private_tree);
        } END_TRANSACTION;
        if (b || bench::early_tx_terminate) {
            break;
        }
    }

    // if we just exited due to time, return now
    if (!b)
        return;

    // ok, we've got a real live tree and it's private.  Let's compute the sum
    // and average values, and return all memory
    un_ptr<RBNode> curr(private_tree);
    // warning: the private tree needs to have a NULL parent, but the way we
    // excised it doesn't guarantee that
    curr->set_parent(sh_ptr<RBNode>(NULL), curr);
    unsigned sum = 0;
    unsigned count = 0;
    while (true) {
        // traverse left if we haven't gone left or right yet
        if ((curr->get_color(curr) != LEFT) &&
            (curr->get_color(curr) != RIGHT))
        {
            curr->set_color(LEFT, curr);
            if (curr->get_child(0, curr) != NULL) {
                curr = curr->get_child(0, curr);
                continue;
            }
        }
        // traverse right if we haven't yet
        if (curr->get_color(curr) != RIGHT) {
            curr->set_color(RIGHT, curr);
            if (curr->get_child(1, curr) != NULL) {
                curr = curr->get_child(1, curr);
                continue;
            }
        }
        // if we've done both sides, then add this to the sum
        sum += curr->get_val(curr);
        count++;
        // store the parent
        sh_ptr<RBNode> p = curr->get_parent(curr);
        // delete the node
        tx_delete(curr);
        // if parent is null, we're finished
        if (p == NULL)
            break;
        curr = p;
    }
}

// remove the node with v as its value if it exists in the tree
void PrivTree::remove(int v)
{
    BEGIN_TRANSACTION;

    // find v
    rd_ptr<RBNode> sentinel_r(sentinel);
    // rename x_r to x_rw, x_rr to x_r
    rd_ptr<RBNode> x_r(sentinel_r->get_child(0, sentinel_r));

    while (x_r != NULL) {
        int xval = x_r->get_val(x_r);
        if (xval == v)
            break;
        x_r = x_r->get_child(v < xval ? 0 : 1, x_r);
    }

    // if we found v, remove it
    if (x_r != NULL) {
        wr_ptr<RBNode> x_rw(x_r); // upgrade to wr_ptr now for best performance

        // ensure that we are deleting a node with at most one child
        // cache value of rhs child
        sh_ptr<RBNode> xrchild(x_rw->get_child(1, x_rw));
        if ((xrchild != NULL) && (x_rw->get_child(0, x_rw) != NULL)) {
            // two kids!  find right child's leftmost child and swap it with x
            rd_ptr<RBNode> leftmost_r(x_rw->get_child(1, x_rw));

            while (leftmost_r->get_child(0, leftmost_r)  != NULL)
                leftmost_r = leftmost_r->get_child(0, leftmost_r);

            x_rw->set_val(leftmost_r->get_val(leftmost_r), x_rw);
            x_rw = leftmost_r;
        }

        // extract x from the tree and prep it for deletion
        wr_ptr<RBNode> parent_rw(x_rw->get_parent(x_rw));
        int cID = (x_rw->get_child(0, x_rw) != NULL) ? 0 : 1;
        sh_ptr<RBNode> child = x_rw->get_child(cID, x_rw);
        // make child the xID'th child of parent
        int xID = x_rw->get_ID(x_rw);
        parent_rw->set_child(xID, child, parent_rw);
        if (child != NULL) {
            wr_ptr<RBNode> child_w(child);
            child_w->set_parent(parent_rw, child_w);
            child_w->set_ID(xID, child_w);
        }

        // fix black height violations
        if ((BLACK == x_rw->get_color(x_rw)) && (child != NULL)) {
            rd_ptr<RBNode> c_r(child);
            if (RED == c_r->get_color(c_r)) {
                wr_ptr<RBNode> c_rw(c_r);
                x_rw->set_color(RED, x_rw);
                c_rw->set_color(BLACK, c_rw);
            }
        }

        // rebalance
        wr_ptr<RBNode> curr(x_rw);
        while (true) {
            parent_rw = curr->get_parent(curr);
            if (((parent_rw == sentinel) || (RED == curr->get_color(curr))))
                break;
            long cID = curr->get_ID(curr);
            wr_ptr<RBNode> sibling_w(parent_rw->get_child(1 - cID, parent_rw));

            /* we'd like y's sibling s to be black
             * if it's not, promote it and recolor
             */
            if (RED == sibling_w->get_color(sibling_w)) {
                /*
                    Bp          Bs
                    / \         / \
                  By  Rs  =>  Rp  B2
                      / \     / \
                     B1 B2  By  B1
                */
                parent_rw->set_color(RED, parent_rw);
                sibling_w->set_color(BLACK, sibling_w);
                // promote sibling
                wr_ptr<RBNode> gparent_w(parent_rw->get_parent(parent_rw));
                int pID = parent_rw->get_ID(parent_rw);
                wr_ptr<RBNode> nephew_w(sibling_w->get_child(cID, sibling_w));
                // set nephew as 1-cID child of parent
                parent_rw->set_child(1-cID, nephew_w, parent_rw);
                nephew_w->set_parent(parent_rw, nephew_w);
                nephew_w->set_ID(1-cID, nephew_w);
                // make parent the cID child of the sibling
                sibling_w->set_child(cID, parent_rw, sibling_w);
                parent_rw->set_parent(sibling_w, parent_rw);
                parent_rw->set_ID(cID, parent_rw);
                // make sibling the pID child of gparent
                gparent_w->set_child(pID, sibling_w, gparent_w);
                sibling_w->set_parent(gparent_w, sibling_w);
                sibling_w->set_ID(pID, sibling_w);
                // reset sibling
                sibling_w = nephew_w;
            }

            sh_ptr<RBNode> n = sibling_w->get_child(1 - cID, sibling_w);
            rd_ptr<RBNode> n_r(n); // if n is null, n_r will be null too
            if ((n != NULL) && (RED == n_r->get_color(n_r))) {
                // the far nephew is red
                wr_ptr<RBNode> n_rw(n);
                /*
                    ?p          ?s
                    / \         / \
                  By  Bs  =>  Bp  Bn
                      / \         / \
                     ?1 Rn      By  ?1
                */
                sibling_w->set_color(parent_rw->get_color(parent_rw), sibling_w);
                parent_rw->set_color(BLACK, parent_rw);
                n_rw->set_color(BLACK, n_rw);
                // promote sibling_w
                wr_ptr<RBNode> gparent_w(parent_rw->get_parent(parent_rw));
                int pID = parent_rw->get_ID(parent_rw);
                sh_ptr<RBNode> nephew(sibling_w->get_child(cID, sibling_w));
                // make nephew the 1-cID child of parent
                parent_rw->set_child(1-cID, nephew, parent_rw);
                if (nephew != NULL) {
                    wr_ptr<RBNode> nephew_w(nephew);
                    nephew_w->set_parent(parent_rw, nephew_w);
                    nephew_w->set_ID(1-cID, nephew_w);
                }
                // make parent the cID child of the sibling
                sibling_w->set_child(cID, parent_rw, sibling_w);
                parent_rw->set_parent(sibling_w, parent_rw);
                parent_rw->set_ID(cID, parent_rw);
                // make sibling the pID child of gparent
                gparent_w->set_child(pID, sibling_w, gparent_w);
                sibling_w->set_parent(gparent_w, sibling_w);
                sibling_w->set_ID(pID, sibling_w);
                break; // problem solved
            }

            n = sibling_w->get_child(cID, sibling_w);
            n_r = n;
            if ((n != NULL) && (RED == n_r->get_color(n_r))) {
                /*
                    ?p          ?p
                    / \         / \
                  By  Bs  =>  By  Bn
                      / \           \
                     Rn B1          Rs
                                      \
                                      B1
                */
                wr_ptr<RBNode> n_rw(n_r);
                sibling_w->set_color(RED, sibling_w);
                n_rw->set_color(BLACK, n_rw);
                wr_ptr<RBNode> t = sibling_w;
                // promote n_rw
                sh_ptr<RBNode> gneph(n_rw->get_child(1-cID, n_rw));
                // make gneph the cID child of sibling
                sibling_w->set_child(cID, gneph, sibling_w);
                if (gneph != NULL) {
                    wr_ptr<RBNode> gneph_w(gneph);
                    gneph_w->set_parent(sibling_w, gneph_w);
                    gneph_w->set_ID(cID, gneph_w);
                }
                // make sibling the 1-cID child of n
                n_rw->set_child(1 - cID, sibling_w, n_rw);
                sibling_w->set_parent(n_rw, sibling_w);
                sibling_w->set_ID(1 - cID, sibling_w);
                // make n the 1-cID child of parent
                parent_rw->set_child(1 - cID, n_rw, parent_rw);
                n_rw->set_parent(parent_rw, n_rw);
                n_rw->set_ID(1 - cID, n_rw);
                sibling_w = n_rw;
                n_rw = t;

                // now the far nephew is red... copy of code from above
                sibling_w->set_color(parent_rw->get_color(parent_rw), sibling_w);
                parent_rw->set_color(BLACK, parent_rw);
                n_rw->set_color(BLACK, n_rw);
                // promote sibling_w
                wr_ptr<RBNode> gparent_w(parent_rw->get_parent(parent_rw));
                int pID = parent_rw->get_ID(parent_rw);
                sh_ptr<RBNode> nephew(sibling_w->get_child(cID, sibling_w));
                // make nephew the 1-cID child of parent
                parent_rw->set_child(1-cID, nephew, parent_rw);
                if (nephew != NULL) {
                    wr_ptr<RBNode> nephew_w(nephew);
                    nephew_w->set_parent(parent_rw, nephew_w);
                    nephew_w->set_ID(1-cID, nephew_w);
                }
                // make parent the cID child of the sibling
                sibling_w->set_child(cID, parent_rw, sibling_w);
                parent_rw->set_parent(sibling_w, parent_rw);
                parent_rw->set_ID(cID, parent_rw);
                // make sibling the pID child of gparent
                gparent_w->set_child(pID, sibling_w, gparent_w);
                sibling_w->set_parent(gparent_w, sibling_w);
                sibling_w->set_ID(pID, sibling_w);

                break; // problem solved
            }
            /*
                ?p          ?p
                / \         / \
              Bx  Bs  =>  Bp  Rs
                  / \         / \
                 B1 B2      B1  B2
            */

            sibling_w->set_color(RED, sibling_w); // propagate problem upwards

            // advance to parent and balance again
            curr = parent_rw;
        }

        // if y was red, this fixes the balance
        curr->set_color(BLACK, curr);

        // free storage associated with deleted node
        tx_delete(x_rw);
    }

    END_TRANSACTION;
}

// returns black-height when balanced and -1 otherwise
int PrivTree::blackHeight(const sh_ptr<RBNode>& x)
{
    if (!x)
        return 0;
    const rd_ptr<RBNode> x_r(x);
    int bh0 = blackHeight(x_r->get_child(0, x_r));
    int bh1 = blackHeight(x_r->get_child(1, x_r));
    if ((bh0 >= 0) && (bh1 == bh0))
        return BLACK==x_r->get_color(x_r) ? 1+bh0 : bh0;
    else
        return -1;
}

// returns true when a red node has a red child
bool PrivTree::redViolation(const rd_ptr<RBNode>& p_r, const sh_ptr<RBNode>& x)
{
    if (!x)
        return false;
    const rd_ptr<RBNode> x_r(x);
    return ((RED == p_r->get_color(p_r) && RED == x_r->get_color(x_r)) ||
            (redViolation(x_r, x_r->get_child(0, x_r))) ||
            (redViolation(x_r, x_r->get_child(1, x_r))));
}

// returns true when all nodes' parent fields point to their parents
bool PrivTree::validParents(const sh_ptr<RBNode>& p, int xID,
                            const sh_ptr<RBNode>& x)
{
    if (!x)
        return true;
    const rd_ptr<RBNode> x_r(x);
    return ((x_r->get_parent(x_r) == p) &&
            (x_r->get_ID(x_r) == xID) &&
            (validParents(x, 0, x_r->get_child(0, x_r))) &&
            (validParents(x, 1, x_r->get_child(1, x_r))));
}

// returns true when the tree is ordered
bool PrivTree::inOrder(const sh_ptr<RBNode>& x, int lowerBound, int upperBound)
{
    if (!x)
        return true;
    const rd_ptr<RBNode> x_r(x);
    return ((lowerBound <= x_r->get_val(x_r)) &&
            (x_r->get_val(x_r) <= upperBound)
            && (inOrder(x_r->get_child(0, x_r), lowerBound,
                        x_r->get_val(x_r) - 1))
            && (inOrder(x_r->get_child(1, x_r),
                        x_r->get_val(x_r) + 1, upperBound)));
}

// build an empty tree
PrivTree::PrivTree(int _max, int _producers)
  : sentinel(new RBNode()),
    m_max(_max),
    m_producers(_producers),
    STACK(new TxStack<sh_ptr<RBNode> >())
{ }

// sanity check of the PrivTree data structure
bool PrivTree::sanity_check() const
{
    bool sane = false;

    BEGIN_TRANSACTION;

    sane = false;
    const rd_ptr<RBNode> sentinel_r(sentinel);
    sh_ptr<RBNode> root = sentinel_r->get_child(0, sentinel_r);

    if (!root) {
        sane = true; // empty tree needs no checks
    }
    else {
        const rd_ptr<RBNode> root_r(root);
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
void PrivTree::printNode(const sh_ptr<RBNode>& x, int indent)
{
    if (!x)
        return;

    const rd_ptr<RBNode> x_r(x);

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
void PrivTree::print(int indent) const
{
    BEGIN_TRANSACTION;

    const rd_ptr<RBNode> sentinel_r(sentinel);
    printNode(sentinel_r->get_child(0, sentinel_r));

    END_TRANSACTION;
}
