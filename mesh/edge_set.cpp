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

/*  edge_set.cpp
 *
 *  Concurrent set of edges.
 *  Good memory locality if your work is confined to a worker stripe.
 *  Has 2*numWorkers hash_sets inside.
 *
 *  Thread-safe insert, erase, and contains methods.
 *  All are currently implemented using pthread_mutex locks.
 *  Insert and erase log to cout if output_incremental.
 *
 *  Non-thread-safe print_all method.
 */

#include <iostream>
    using std::cout;

#include "config.hpp"
#include "point.hpp"
#include "edge_set.hpp"

template<class edgeRp>
    // may be instantiated with edge::Rp or edge::Up
static int segment(edgeRp  e) {
    point* p0 = e->get_points(0, e);
    point* p1 = e->get_points(1, e);

    int a = bucket(p0);
    int b = bucket(p1);
    return a < b ? a : b;
}

template<class edgeRp>
    // may be instantiated with edge::Rp or edge::Up
void edge_set::insert(edgeRp e) {
    int s = segment(e);
    if (txnal<edgeRp>()) {
        segments[s]->tx_insert(e);
    } else {
        segments[s]->pv_insert(e);
    }
    if (output_incremental) {
        with_lock cs(io_lock);
        cout << "+ ";
        e->print();
    }
}
// explicit instantiations:
template void edge_set::insert<edge::Rp>(edge::Rp);
template void edge_set::insert<edge::Up>(edge::Up);

template<class edgeRp>
    // may be instantiated with edge::Rp or edge::Up
    // edge::Wp is also ok, if that's what you have
void edge_set::erase(edgeRp e) {
    int s = segment(e);
    if (txnal<edgeRp>()) {
        segments[s]->tx_remove(e);
    } else {
        segments[s]->pv_remove(e);
    }
    if (output_incremental) {
        with_lock cs(io_lock);
        cout << "- ";
        e->print();
    }
}
// explicit instantiations:
template void edge_set::erase<edge::Rp>(edge::Rp);
template void edge_set::erase<edge::Wp>(edge::Wp);
template void edge_set::erase<edge::Up>(edge::Up);

template<class edgeRp>
    // may be instantiated with edge::Rp or edge::Up
bool edge_set::contains(edgeRp e) const {
    int s = segment(e);
    if (txnal<edgeRp>()) {
        return segments[s]->tx_lookup(e);
    } else {
        return segments[s]->pv_lookup(e);
    }
}
// explicit instantiation (not actually used in the Delaunay
// app, but would be needed if anybody called /contains/)
template bool edge_set::contains<edge::Up>(edge::Up) const;
template bool edge_set::contains<edge::Rp>(edge::Rp) const;

void print_edge(edge::Sp e) { cout << "+ "; (edge::Up(e))->print(); }

void edge_set::print_all() const {
    for (int i = 0; i < num_workers*2; i++) {
        segments[i]->pv_apply_to_all(print_edge);
    }
}

edge_set::edge_set() {
    segments = new segment_t*[num_workers*2];
        // no initialization performed for pointers (non-class type)
}

void edge_set::help_initialize(int col) {
    // There will be 3*(n-1)-h edges in the final graph,
    // where n is num_points and h is the number of points
    // on the convex hull.  Factor of 4 here gives us an
    // expected load factor of just under 3/4
    col <<= 1;
    segments[col] = new segment_t((4*num_points)/num_workers/2);
    segments[col+1] = new segment_t((4*num_points)/num_workers/2);
}

edge_set::~edge_set() {
    for (int i = 0; i < num_workers; i++) {
        delete segments[i];
    }
    delete[] segments;
}
