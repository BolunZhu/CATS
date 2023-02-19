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

/*  side.hpp
 *
 *  Data structure to represent one side of a stitching-up operation.
 *  Contains methods to move around the border of a convex hull.
 */

#ifndef SIDE_H
#define SIDE_H

#include "config.hpp"
#include "point.hpp"
#include "edge.hpp"

// Point p is assumed to lie on a convex hull.  Return the edge that
// lies dir of p on the hull.
// Can be instantiated with <rd_ptr> or <un_ptr>
//
template <template <class> class rdp>
rdp<edge> hull_edge(point* p, int dir);

struct pv_side {
    point* p;     // working point
    edge::Up a;   // where we came from
    edge::Up b;   // where we're going to
    point* ap;    // at far end of a
    point* bp;    // at far end of b
    int ai;       // index of p within a
    int bi;       // index of p within b
    int dir;      // which way are we moving?

    // No non-trivial constructor.

    // pt is a point on a convex hull.
    // Initialize such that a and b are the the edges with the
    // external angle, and b is d (ccw, cw) of a.
    //
    void initialize(point* pt, int d);

    // e is an edge between two regions in the process of being
    // stitched up.  pt is an endpoint of e.  After reinitialization,
    // a should be e, ai should be index_of(e, pt), and b should be d
    // of a at end ai.
    //
    void reinitialize(edge::Up e, point* pt, int d);

    // Nearby edges may have been updated.  Make sure a and
    // b still flank endpoints of mid in direction d.
    // This means that mid _becomes_ a.
    //
    void update(edge::Up mid, int d);

    // Move one edge along a convex hull (possibly with an external
    // edge attached at p), provided we stay in sight of point o.
    // Return edge we moved across, or null if unable to move.
    //
    edge::Up move(point* o);

    // We're stitching up a seam.  'This' captures one side of
    // current base edge (not explicitly needed).  Edge bottom is at
    // the "bottom" (initial end) of the seam; tells us if we cycle all
    // the way around.  Point o is at other end of base.  Find candidate
    // endpoint for new edge on this side, moving "up" the seam.
    // Break edges as necessary.
    //
    point* find_candidate(edge::Up bottom, point* o);

    // Move one edge along a convex hull (possibly with an external
    // edge attached at p), provided we stay in sight of point o
    // and don't get too close to the next seam.
    // Return edge we moved across, or null if we didn't move.
    //
    edge::Up conditional_move(point* o, int seam);
};

struct tx_side {
    point* p;     // working point
    edge::Rp b;   // where we're going to
    point* bp;    // at far end of b
    int bi;       // index of p within b
    int dir;      // which way are we moving?

    // No non-trivial constructor.

    // e is an edge between two regions in the process of being
    // stitched up.  pt is an endpoint of e.  After initialization,
    // b should be d of e at the p end, bi should be the p end of b,
    // and bp should be at the other end of b.
    //
    void initialize(edge::Rp e, point* pt, int d);

    // Move one edge along a convex hull (possibly with an external
    // edge attached at p), provided we stay in sight of point o.
    // Return edge we moved across, or null if unable to move.
    //
    edge::Sp move(point* o);
};

#endif
