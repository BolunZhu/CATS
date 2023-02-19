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

/*  side.cpp
 *
 *  Data structure to represent one side of a stitching-up operation.
 *  Contains methods to move around the border of a convex hull.
 */

#include "side.hpp"

// Point p is assumed to lie on a convex hull.  Return the edge that
// lies dir of p on the hull.
// Can be instantiated with <edge::Rp> or <edge::Up>
//
template <template <class> class rdp>
rdp<edge> hull_edge(point* p, int dir)
{
    sh_ptr<edge> first_edge(p->get_first_edge<rdp>());

    if (first_edge == 0) {
        return rdp<edge>();    // NULL
    }
    else {
        rdp<edge> a(first_edge);
        int ai = index_of(a, p);
        point* ap = a->get_points(1-ai, a);
        if (a->get_neighbors(ai, dir, a) == a) {
            // only one incident edge at p
            return a;
        } else {
            // >= 2 incident edges at p;
            // need to find correct ones
            rdp<edge> b;
            while (true) {
                b = a->get_neighbors(ai, dir, a);
                int bi = index_of(b, p);
                point* bp = b->get_points(1-bi, b);
                if (extern_angle(ap, p, bp, dir))
                    return b;
                a = b;
                ai = bi;
                ap = bp;
            }
        }
    }
}
// explicit instantiations:
template rd_ptr<edge> hull_edge<rd_ptr>(point*, int);
template un_ptr<edge> hull_edge<un_ptr>(point*, int);

// pt is a point on a convex hull.
// Initialize such that a and b are the edges with the
// external angle, and b is dir (ccw, cw) of a.
//
void pv_side::initialize(point* pt, int d) {
    p = pt;
    dir = d;
    b = hull_edge<un_ptr>(pt, d);
    if (b == 0) {
        a = un_ptr<edge>();
        ap = 0;  bp = 0;
        ai = bi = 0;
    } else {
        bi = index_of(b, p);
        bp = b->get_points(1-bi, b);
        a = b->get_neighbors(bi, 1-dir, b);
        ai = index_of(a, p);
        ap = a->get_points(1-bi, a);
    }
}

// e is an edge between two regions in the process of being
// stitched up.  pt is an endpoint of e.  After initialization,
// b should be d of e at the p end, bi should be the p end of b,
// and bp should be at the other end of b.
//
void tx_side::initialize(edge::Rp e, point* pt, int d) {
    p = pt;
    dir = d;
    b = e->get_neighbors(index_of(e, p), dir, e);
    bi = index_of(b, p);
    bp = b->get_points(1-bi, b);
}

// Nearby edges may have been updated.  Make sure a and
// b still flank endpoints of mid in direction dir.  This
// means that mid _becomes_ a.
//
void pv_side::update(edge::Up mid, int d) {
    dir = d;
    a = mid;
    ai = index_of(mid, p);
    b = mid->get_neighbors(ai, dir, mid);
    bi = index_of(b, p);
    ap = mid->get_points(1-ai, mid);
    bp = b->get_points(1-bi, b);
}

// e is an edge between two regions in the process of being
// stitched up.  pt is an endpoint of e.  After initialization,
// a should be e, ai should be index_of(e, pt), and b should be dir
// of a at end ai.
//
void pv_side::reinitialize(edge::Up e, point* pt, int d) {
    p = pt;
    update(e, d);
}

// Move one edge along a convex hull (possibly with an external
// edge attached at p), provided we stay in sight of point o.
// Return edge we moved across, or null if unable to move.
//
edge::Up pv_side::move(point* o) {
    if (b != 0 && bp != o && !extern_angle(bp, p, o, 1-dir)) {
        a = b;
        ai = 1-bi;
        ap = p;
        p = b->get_points(1-bi, b);
        b = b->get_neighbors(1-bi, dir, b);
        bi = index_of(b, p);
        bp = b->get_points(1-bi, b);
        return a;
    }
    return edge::Up();                  // NULL
}
edge::Sp tx_side::move(point* o) {
    if (b != 0 && bp != o && !extern_angle(bp, p, o, 1-dir)) {
        edge::Sp a = b;
        p = b->get_points(1-bi, b);
        b = b->get_neighbors(1-bi, dir, b);
        bi = index_of(b, p);
        bp = b->get_points(1-bi, b);
        return a;
    }
    return edge::Sp();      // NULL
}

// Move one edge along a convex hull (possibly with an external
// edge attached at p), provided we stay in sight of point o
// and don't get too close to the next seam.
// Return edge we moved across, or null if we didn't move.
//
edge::Up pv_side::conditional_move(point* o, int seam) {
    if (b != 0
            && bp != o  // only adjacent edge is the external one
            && closest_seam(o) == seam
            && closest_seam(p) == seam
            && closest_seam(bp) == seam
            && !extern_angle(bp, p, o, 1-dir)) {
        a = b;
        ai = 1-bi;
        ap = p;
        p = bp;
        b = b->get_neighbors(1-bi, dir, b);
        bi = index_of(b, p);
        bp = b->get_points(1-bi, b);
        return a;
    }
    return edge::Up();                  // NULL
}

// We're stitching up a seam.  'This' captures one side of
// current base edge (not explicitly needed).  Edge bottom is at
// the "bottom" (initial end) of the seam; tells us if we cycle all
// the way around.  Point o is at other end of base.  Find candidate
// endpoint for new edge on this side, moving "up" the seam.
// Break edges as necessary.
//
point* pv_side::find_candidate(edge::Up bottom, point* o) {
    if (a == bottom) {
        // no more candidates on this side
        return 0;
    }
    point* c = a->get_points(1-ai, a);
    if (extern_angle(o, p, c, 1-dir)) {
        // no more candidates
        return 0;
    }
    while (true) {
        edge::Up na(a->get_neighbors(ai, 1-dir, a));
            // next edge into region
        if (na == bottom) {
            // wrapped all the way around
            return c;
        }
        int nai = index_of(na, p);
        point* nc = na->get_points(1-nai, na);
            // next potential candidate
        if (encircled(o, c, p, nc, 1-dir)) {
            // have to break an edge
            destroy(a);
            a = na;
            ai = nai;
            c = nc;
        } else return c;
    }
}
