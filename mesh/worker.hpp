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

/*  worker.hpp
 *
 *  Main file for the parallel solver.
 */

#ifndef WORKER_H
#define WORKER_H

#include "config.hpp"
#include "point.hpp"
#include "edge.hpp"
#include "queues.hpp"
#include "my_thread.hpp"
#include "barrier.hpp"

// Everything regions (vertical stripes) need to know about each other.
//
struct region_info {
    point** points;
    int *counts;
    int npts;
    point* leftmost;
    point* rightmost;
    simple_queue<edge::Sp>* tentative_edges;

    region_info(const int tid) {
        tentative_edges = new concurrent_queue<edge::Sp>(tid);
        points = new point*[num_workers];
        counts = new int[num_workers];
        npts = 0;
        leftmost = rightmost = 0;
    }
    ~region_info() {
        delete[] points;
        delete[] counts;
    }
};

class worker : public runnable {
    int col;
    region_info **regions;
    barrier *bar;
public:
    virtual void operator()();
    worker(int c, region_info **r, barrier *b)
        : col(c), regions(r), bar(b) { }
};

#endif
