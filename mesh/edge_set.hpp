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

/*  edge_set.hpp
 *
 *  Concurrent set of edges.
 *  Good memory locality if your work is confined to a worker stripe.
 *
 *  Thread-safe insert, erase, and contains methods.
 *  Insert and erase log to cout if output_incremental.
 *
 *  Non-thread-safe print_all method.
 */

#ifndef EDGE_SET_H
#define EDGE_SET_H

#include "tm_hash_set.hpp"
#include "edge.hpp"

class edge_set {
    typedef tm_hash_set<edge::Sp> segment_t;
    segment_t** segments;
public:
    // Edge set decides whether to synchronize access to the set itself
    // based on whether it is passed a rd_ptr or an un_ptr
    // (print_all is unsynchronized).

    template<class edgeRp>
        // may be instantiated with edge::Rp or edge::Up
    void insert(edgeRp e);

    template<class edgeRp>
        // may be instantiated with edge::Rp or edge::Up
    void erase(edgeRp e);

    template<class edgeRp>
        // may be instantiated with edge::Rp or edge::Up
    bool contains(edgeRp e) const;

    void print_all() const;

    // for debugging:
    void print_stats() const {
        for (int s = 0; s < num_workers; s++) {
            cout << "segment " << s << ":";
            segments[s]->print_stats();
        }
    }

    edge_set();
        // constructor creates segments but does not initialize them
    void help_initialize(int col);
        // initialize segments in parallel with other threads

    ~edge_set();
};

extern edge_set *edges;

#endif
