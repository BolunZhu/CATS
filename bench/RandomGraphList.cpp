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
#include "RandomGraphList.hpp"

#ifdef _MSC_VER
#include <alt-license/rand_r.h>
#endif

using namespace stm;
using namespace bench;
using std::cout;
using std::endl;


// This is the entry point for an insert tx from the benchmark harness
void RandomGraphList::insert(int val, unsigned int* seed)
{
    // trim val down to the appropriate range
    val %= maxNodes;

    // get the rand linkups outside of the tx, so that the tx doesn't succeed
    // by retrying until it gets a lucky set of values
    int * linkups = new int[default_linkups];

    // NB: if we sorted this list, we could make a single pass for doing
    // linkups
    int q = 0;
    while (q < default_linkups) {
        linkups[q] = rand_r(seed) % maxNodes;
        // make sure we don't link to self
        if (linkups[q] == val)
            continue;
        bool ok = true;
        // make sure we don't link to the same thing twice
        for (int qq = 0; qq < q; qq++) {
            if (linkups[q] == linkups[qq])
                ok = false;
        }
        if (ok)
            q++;
    }

    BEGIN_TRANSACTION {
        if (!bench::early_tx_terminate) {
            // search for the node we want to insert
            rd_ptr<VListNode> r_prev(nodes);
            rd_ptr<VListNode> r_curr(r_prev->get_next(r_prev));
            bool do_insert = true;
            while (r_curr) {
                // look at curr's contents
                rd_ptr<Vertex> r_vtx(r_curr->get_vertex(r_curr));
                if (r_vtx->get_val(r_vtx) == val) {
                    // val is already in the list, so end this attempt
                    do_insert = false;
                    break;
                }
                if (r_vtx->get_val(r_vtx) > val) {
                    // val is not in the list, so break to the insertion code
                    do_insert = true;
                    break;
                }
                // advance to next
                r_prev = r_curr;
                r_curr = r_curr->get_next(r_curr);
            }
            if (do_insert) {
                // create the node
                sh_ptr<Vertex> new_vertex(new Vertex(val));
                // now create a listnode to hold it
                sh_ptr<VListNode>
                    new_vertex_node(new VListNode(new_vertex, r_curr));
                // now link the listnode into the main list
                wr_ptr<VListNode> w_prev(r_prev);
                w_prev->set_next(new_vertex_node, w_prev);

                // now we need to perform linkups
                for (int i = 0; i < default_linkups; i++) {
                    // search list for this node
                    rd_ptr<VListNode> list_iterator(nodes);
                    list_iterator = list_iterator->get_next(list_iterator);
                    while (list_iterator) {
                        rd_ptr<Vertex>
                            it_contents(list_iterator->get_vertex(list_iterator));
                        if (it_contents->get_val(it_contents) == linkups[i]) {
                            // connect it_contents to new_vertex
                            {
                                rd_ptr<VListNode> r_prev(it_contents->get_alist(it_contents));
                                rd_ptr<VListNode> r_curr(r_prev->get_next(r_prev));
                                while (r_curr) {
                                    rd_ptr<Vertex> contents(r_curr->get_vertex(r_curr));
                                    if (contents->get_val(contents) > val)
                                        break;
                                    r_prev = r_curr;
                                    r_curr = r_curr->get_next(r_curr);
                                }
                                // create new node
                                sh_ptr<VListNode> new_v(new VListNode(new_vertex, r_curr));
                                wr_ptr<VListNode> w_prev(r_prev);
                                w_prev->set_next(new_v, w_prev);
                            }
                            // connect new_vertex to it_contents
                            {
                                rd_ptr<Vertex> r_new_vtx(new_vertex);
                                rd_ptr<VListNode> r_prev(r_new_vtx->get_alist(r_new_vtx));
                                rd_ptr<VListNode> r_curr(r_prev->get_next(r_prev));
                                rd_ptr<Vertex> contents;
                                while (r_curr) {
                                    contents = r_curr->get_vertex(r_curr);
                                    if (contents->get_val(contents) > linkups[i])
                                        break;
                                    r_prev = r_curr;
                                    r_curr = r_curr->get_next(r_curr);
                                }
                                // create new node
                                sh_ptr<VListNode> new_v(new VListNode(it_contents, r_curr));
                                wr_ptr<VListNode> w_prev(r_prev);
                                w_prev->set_next(new_v, w_prev);
                            }
                            break;
                        }
                        list_iterator = list_iterator->get_next(list_iterator);
                    }
                }
            }
        }
    } END_TRANSACTION;

    delete linkups;
}

// print the RandomGraph structure
void RandomGraphList::print() const
{
    BEGIN_TRANSACTION {
        cout << "graph:" << endl;
        rd_ptr<VListNode> curr(nodes);
        curr = curr->get_next(curr);
        while (curr) {
            rd_ptr<Vertex> v(curr->get_vertex(curr));
            cout << v->get_val(v);
            rd_ptr<VListNode> q(v->get_alist(v));
            q = q->get_next(q);
            while (q) {
                rd_ptr<Vertex> qq(q->get_vertex(q));
                cout << "::" << qq->get_val(qq);
                q = q->get_next(q);
            }
            cout << endl;
            curr = curr->get_next(curr);
        }
    } END_TRANSACTION;
}

// This is the entry point for a remove tx from the benchmark harness
void RandomGraphList::remove(int val)
{
    // trim val down to the appropriate range
    val %= maxNodes;
    BEGIN_TRANSACTION {
        if (!bench::early_tx_terminate) {
            // search for the node we want to remove
            rd_ptr<VListNode> r_prev(nodes);
            rd_ptr<VListNode> r_curr(r_prev->get_next(r_prev));
            bool do_remove = false;
            while (r_curr) {
                // look at curr's contents
                rd_ptr<Vertex> r_vtx(r_curr->get_vertex(r_curr));
                if (r_vtx->get_val(r_vtx) == val) {
                    // val is already in the list, so drop to deletion code
                    do_remove = true;
                    break;
                }
                if (r_vtx->get_val(r_vtx) > val) {
                    // val is not in the list, so end this attempt
                    do_remove = false;
                    break;
                }
                // advance to next
                r_prev = r_curr;
                r_curr = r_curr->get_next(r_curr);
            }
            if (do_remove) {
                // first unlink the node
                sh_ptr<Vertex> vtx(r_curr->get_vertex(r_curr));
                wr_ptr<VListNode> w_prev(r_prev);
                w_prev->set_next(r_curr->get_next(r_curr), w_prev);
                // delete the list node holding the vertex we're destroying
                tx_delete(r_curr);

                // now for each link in the vertex, disconnect the remote link
                {
                    rd_ptr<Vertex> r_vtx(vtx);
                    rd_ptr<VListNode> r_alist(r_vtx->get_alist(r_vtx));
                    r_alist = r_alist->get_next(r_alist);
                    while (r_alist) {
                        // open the adjacent vertex for reading
                        rd_ptr<Vertex> r_nbor(r_alist->get_vertex(r_alist));
                        // remove the nbor's backlink to vtx
                        rd_ptr<VListNode> r_prev(r_nbor->get_alist(r_nbor));
                        rd_ptr<VListNode> r_curr(r_prev->get_next(r_prev));
                        while (r_curr) {
                            rd_ptr<Vertex> rvi(r_curr->get_vertex(r_curr));
                            if (rvi->get_val(rvi) == r_vtx->get_val(r_vtx)) {
                                // found backlink.  kill it
                                wr_ptr<VListNode> w_prev(r_prev);
                                w_prev->set_next(r_curr->get_next(r_curr), w_prev);
                                tx_delete(r_curr);
                                break;
                            }
                            r_prev = r_curr;
                            r_curr = r_curr->get_next(r_curr);
                        }

                        // advance to next adjacent vertex
                        r_alist = r_alist->get_next(r_alist);
                    }
                }
                // now free all memory for this vertex
                {
                    rd_ptr<Vertex> r_vtx(vtx);
                    rd_ptr<VListNode> r_prev(r_vtx->get_alist(r_vtx));
                    rd_ptr<VListNode> r_curr(r_prev->get_next(r_prev));
                    while (r_curr) {
                        tx_delete(r_prev);
                        r_prev = r_curr;
                        r_curr = r_curr->get_next(r_curr);
                    }
                    tx_delete(r_prev);
                    tx_delete(r_vtx);
                }
            } // do_remove
        }
    } END_TRANSACTION;
}

// sanity check of the RandomGraph structure... let's not use a transaction...
bool
RandomGraphList::isSane() const
{
    // outer loop:  make sure that the main list of nodes is sorted
    un_ptr<VListNode> curr(nodes);
    curr = curr->get_next(curr);

    // step 1:  make sure the outer list is sorted and has no duplicates
    int prev = -1;
    while (curr) {
        un_ptr<Vertex> c(curr->get_vertex(curr));
        if (c->get_val(c) <= prev)
            return false;
        prev = c->get_val(c);
        curr = curr->get_next(curr);
    }

    // step 2:  make sure the vertex adjacency lists are sorted
    curr = nodes;
    curr = curr->get_next(curr);
    while (curr) {
        un_ptr<Vertex> c(curr->get_vertex(curr));
        un_ptr<VListNode> ca(c->get_alist(c));
        ca = ca->get_next(ca);
        prev = -1;
        while (ca) {
            un_ptr<Vertex> cac(ca->get_vertex(ca));
            if (cac->get_val(cac) <= prev)
                return false;
            prev = cac->get_val(cac);
            ca = ca->get_next(ca);
        }
        curr = curr->get_next(curr);
    }

    // step 3: make sure every alist entry has a backlink, and that every alist
    // entry is in the main list
    curr = nodes;
    curr = curr->get_next(curr);
    while (curr) {
        un_ptr<Vertex> cv(curr->get_vertex(curr)); // current's vertex
        un_ptr<VListNode> cva(cv->get_alist(cv));  // cv's alist
        cva = cva->get_next(cva);
        while (cva) {
            // check for backlink
            un_ptr<Vertex> nv(cva->get_vertex(cva));
            un_ptr<VListNode> nva(nv->get_alist(nv));
            nva = nva->get_next(nva);
            bool found = false;
            while (nva) {
                un_ptr<Vertex> vv(nva->get_vertex(nva));
                if (vv->get_val(vv) == cv->get_val(cv)) {
                    found = true;
                    break;
                }
                nva = nva->get_next(nva);
            }
            if (!found)
                return false; // no backlink
            // make sure nv is in the main list
            bool inlist = false;
            un_ptr<VListNode> ml(nodes); // main list
            ml = ml->get_next(ml);
            while (ml) {
                un_ptr<Vertex> mlv(ml->get_vertex(ml));
                if (mlv->get_val(mlv) == nv->get_val(nv)) {
                    inlist = true;
                    break;
                }
                ml = ml->get_next(ml);
            }
            if (!inlist)
                return false; // linked to entry not in list
            cva = cva->get_next(cva);
        }
        curr = curr->get_next(curr);
    }

    return true;
}

