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

/*  worker.cpp
 *
 *  Main file for the parallel solver.
 */

#include <vector>
    using std::vector;
#include <limits.h>     // for INT_MIN, INT_MAX
#include "config.hpp"
#include "point.hpp"
#include "edge.hpp"
#include "side.hpp"
#include "lock.hpp"
#include "barrier.hpp"
#include "dwyer.hpp"
#include "edge_set.hpp"
#include "worker.hpp"

// Do quick stitch-up of the guaranteed-private portion
// of a seam between regions.  Returns edge at which it
// stopped.
//
static edge::Sp baste(pv_side &left, pv_side &right,
            const int dir, const int col,
            edge::Up starter, simple_queue<edge::Sp>* tentative_edges)
{
    edge::Up cur_edge = starter;
    while (1) {
        int ly = left.p->coord[ydim];
        int ry = right.p->coord[ydim];
        edge::Up traversed;
        if (dir == ccw ? (ly > ry) : (ly < ry)) {
            // prefer to move on the left
            traversed = left.conditional_move(right.p, col);
            if (traversed == 0) {
                traversed = right.conditional_move(left.p, col);
            }
        } else {
            // prefer to move on the right
            traversed = right.conditional_move(left.p, col);
            if (traversed == 0) {
                traversed = left.conditional_move(right.p, col);
            }
        }
        if (traversed == 0) break;
        edge::Sp ep;
        edge::create(left.p, right.p, left.b, right.b, dir, ep);
        cur_edge = ep;
        cur_edge->set_tentative(true, cur_edge);
        tentative_edges->enqueue(ep, col);
        if (!traversed->get_tentative(traversed)) {
            // It's possible, though unlikely, that traversed edge lies
            // entirely in the upper half of my seam, but was traversed (on
            // the other side) during the lower synchronized baste of my neighbor
            traversed->set_tentative(true, traversed);
            tentative_edges->enqueue(traversed, col);
        }
    }
    return cur_edge;
}

// Similar to regular baster, but for the portion of the
// seam beyond the guaranteed-to-be-private part.
//
static void synchronized_baste(point* lp, point* rp, int dir,
        const int col, edge::Sp cur_edge, region_info **regions)
#ifndef FGL
{
    bool done = false;
    while (!done) {
        edge::Sp new_edge;
        edge::Sp traversed;
        point* tx_lp = NULL;
        point* tx_rp = NULL;

        BEGIN_TRANSACTION;
            tx_lp = NULL;
            tx_rp = NULL;

            tx_side left;
            tx_side right;

            done = false;
            edge::Rp ce_r(cur_edge);
            left.initialize(ce_r, lp, 1-dir);
            right.initialize(ce_r, rp, dir);
            if (left.bp == right.bp) {
                // I've been boxed in by a neighbor
                done = true;
            } else {
                int ly = left.p->coord[ydim];
                int ry = right.p->coord[ydim];
                if (dir == ccw ? (ly > ry) : (ly < ry)) {
                    // prefer to move on the left
                    traversed = left.move(right.p);
                    if (traversed == 0) {
                        traversed = right.move(left.p);
                    }
                } else {
                    // prefer to move on the right
                    traversed = right.move(left.p);
                    if (traversed == 0) {
                        traversed = left.move(right.p);
                    }
                }
                if (traversed == 0) {
                    // can't move
                    done = true;
                } else {
                    edge::Wp lw(left.b);
                    edge::Wp rw(right.b);
                    edge::create(left.p, right.p, lw, rw, dir, new_edge);
                    edge::Wp ne_w(new_edge);
                    edge::Wp tr_w(traversed);
                    ne_w->set_tentative(true, ne_w);
                    if (tr_w->get_tentative(tr_w)) {
                        traversed = edge::Sp();  // don't enqueue twice
                    } else {
                        tr_w->set_tentative(true, tr_w);
                    }
                }
            }
            if (!done) {
                tx_lp = left.p;
                tx_rp = right.p;
            }
        END_TRANSACTION;

        if (done) return;
        lp = tx_lp;             // These three variables are loop-carried.
        rp = tx_rp;             // They cannot safely be updated until
        cur_edge = new_edge;    // after the transaction commits.
        // Similarly, enqueues should be done only if the transaction commits.
        regions[col]->tentative_edges->enqueue(new_edge, col);
        if (traversed != 0)
            regions[col]->tentative_edges->enqueue(traversed, col);
    }
}

#else  // FGL

{
    class inconsistency {};     // exception
    edge::Up cur(cur_edge);
    while (1) {  // retry if inconsistency encountered
        try {
            while (1) {
                pv_side left;   left.reinitialize(cur, lp, 1-dir);
                pv_side right;  right.reinitialize(cur, rp, dir);
                if (left.bp == right.bp) {
                    // I've been boxed in by a neighbor
                    return;
                }
                int ly = left.p->coord[ydim];
                int ry = right.p->coord[ydim];
                edge::Up traversed;     // initialized NULL
                if (dir == ccw ? (ly > ry) : (ly < ry)) {
                    // prefer to move on the left
                    traversed = left.move(right.p);
                    if (traversed == 0) {
                        traversed = right.move(left.p);
                    }
                } else {
                    // prefer to move on the right
                    traversed = right.move(left.p);
                    if (traversed == 0) {
                        traversed = left.move(right.p);
                    }
                }
                if (traversed == 0) return;     // can't move
                {
                    point_set S;
                    with_locked_points cs(S | lp | rp | left.p | right.p);
                        // two of those will be the same, but that's ok
                    if (left.a->get_neighbors(
                                    left.ai, 1-dir, left.a()) != left.b
                            || right.a->get_neighbors(
                                    right.ai, dir, right.a()) != right.b
                            || left.a->get_neighbors(
                                    1-left.ai, dir, left.a()) != right.a) {
                        // inconsistent; somebody else has been here
                        throw inconsistency();
                    }
                    edge::create(left.p, right.p, left.b, right.b, dir,
                                 cur_edge);
                    cur = cur_edge;
                    cur->set_tentative(true);
                    regions[col]->tentative_edges->enqueue(cur_edge, col);
                    if (!traversed->get_tentative(traversed())) {
                        traversed->set_tentative(true);
                        regions[col]->tentative_edges->enqueue(traversed, col);
                    }
                }
                lp = left.p;
                rp = right.p;
            }
        } catch (inconsistency) {}
    }
}
#endif  // FGL

//  Utility routine to avoid typing the body 3 times:
//
static void reconsider_edge(edge::Sp e, const int col, region_info **regions) {
#ifdef FGL
    edge::Up e_w(e);
    synchronized_reconsider(e_w, col, regions[col]->tentative_edges);
#else
    edge::Sp e1;  edge::Sp e2;
    edge::Sp e3;  edge::Sp e4;
    BEGIN_TRANSACTION;
        edge::Wp e_w(e);
        bool stat __attribute__((unused)) =
            reconsider<edge::Rp, edge::Wp>(e_w, col, e1, e2, e3, e4);
        assert (stat);
    END_TRANSACTION;
    if (e1 != 0) regions[col]->tentative_edges->enqueue(e1, col);
    if (e2 != 0) regions[col]->tentative_edges->enqueue(e2, col);
    if (e3 != 0) regions[col]->tentative_edges->enqueue(e3, col);
    if (e4 != 0) regions[col]->tentative_edges->enqueue(e4, col);
#endif
}

//  Main routine for workers.
//  Called from runnable worker.
//  Assume num_workers > 1.
//
static void do_work(const int col, region_info **regions, barrier *bar)
{
    static vector<point> sorted_points;         // shared temporary vector,
                                                // does not need to be
                                                // synchronized (see algorithm)

    int my_start = 0;                           // index in sorted_points

    // Figure out how many of each of my peers' points I have.
    // No synchronization required.

    regions[col] = new region_info(col);

    for (int i = 0; i < num_workers; i++) {
        regions[col]->counts[i] = 0;
    }
    for (int i = (num_points * col) / num_workers;
             i < (num_points * (col+1)) / num_workers; i++) {
        regions[col]->counts[stripe(&all_points[i])]++;
    }
    if (col == 0) {
        resize(sorted_points, num_points);      // One of the workers resizes
    }
    edges->help_initialize(col);

    bar->wait("");

    // Give appropriate points to peers.
    // No synchronization required.

    int peer_start[num_workers];
    int cursor = 0;
    // for each region:
    for (int i = 0; i < num_workers; i++) {
        if (i == col) {
            my_start = cursor;
        }
        // for all peers to the left:
        for (int j = 0; j < col; j++) {
            cursor += regions[j]->counts[i];
        }
        peer_start[i] = cursor;
            // beginning of points in region i supplied by me
        // for all peers to the right:
        for (int j = col; j < num_workers; j++) {
            cursor += regions[j]->counts[i];
        }
        if (i == col) {
            regions[col]->npts = cursor - my_start;
        }
    }

    for (int i = (num_points * col) / num_workers;
             i < (num_points * (col+1)) / num_workers; i++) {
        sorted_points[peer_start[stripe(&all_points[i])]++]
            = all_points[i];
    }

    bar->wait("");

    if (col == 0) {
        swap(all_points, sorted_points);
        resize(sorted_points, 0);
    }

    bar->wait("point partitioning");

    // Triangulate my region (vertical stripe).
    // No synchronization required.

    // Find my extreme values:
    int miny = -(2 << (MAX_COORD_BITS-1));      // close enough
    int maxy = (2 << (MAX_COORD_BITS-1)) - 1;   // close enough
    int minx = INT_MAX;
    int maxx = INT_MIN;
    int t;
    for (int i = my_start; i < my_start + regions[col]->npts; i++) {
        if ((t = all_points[i].coord[xdim]) < minx) minx = t;
        if ((t = all_points[i].coord[xdim]) > maxx) maxx = t;
    }

    if (regions[col]->npts > 0) {
        dwyer_solve(all_points,
                    my_start, my_start + regions[col]->npts - 1,
                    minx, maxx, miny, maxy, 0);
    }

    bar->wait("Dwyer triangulation");

    // Find my extreme points (have to do this _after_ dwyer_solve;
    // it moves points):
    for (int i = my_start; i < my_start + regions[col]->npts; i++) {
        if (all_points[i].coord[xdim] == minx) {
            regions[col]->leftmost = &all_points[i];
        }
        if (all_points[i].coord[xdim] == maxx) {
            regions[col]->rightmost = &all_points[i];
        }
    }

    bar->wait("");

    int next_col = col + 1;
    while (next_col < num_workers &&
            regions[next_col]->npts == 0) {
        ++next_col;
    }
    int neighbor = next_col;
    bool have_seam = regions[col]->npts != 0 && neighbor < num_workers;

    edge::Sp starter_s;     // initialized NULL

    // create initial edge between regions.
    // Must be synchronized to accommodate singleton regions.

    if (have_seam) {
        // Connect rightmost point in my region with leftmost
        // point in the region to my right.
        point* lp = regions[col]->rightmost;
        point* rp = regions[neighbor]->leftmost;
#ifdef FGL
            point_set S;
            with_locked_points cs(S | lp | rp);
            edge::Up lb(hull_edge<rd_ptr>(lp, cw));
            edge::Up rb(hull_edge<rd_ptr>(rp, ccw));
            edge::create(lp, rp, lb, rb, ccw, starter_s);
#else
        BEGIN_TRANSACTION;
            edge::Wp lb(hull_edge<rd_ptr>(lp, cw));
            edge::Wp rb(hull_edge<rd_ptr>(rp, ccw));
            edge::create(lp, rp, lb, rb, ccw, starter_s);
        END_TRANSACTION;
#endif
    }

    bar->wait("initial cross edges");

    edge::Up starter(starter_s);    // privatization

    // As of now, every point has at least one edge, and I will break
    // no edges until the reconsideration phase.  Therefore points could
    // (temporarily) be thought of as immutable, and accessed via un_ptrs,
    // even if they still had first_edges fields (which they don't).
    // I don't currently exploit this fact.

    edge::Sp cur_edge;      // initialized NULL
    pv_side left;  pv_side right;
    sequential_queue<edge::Sp> my_tentative_edges;

    // Stitch up the guaranteed-private lower part of
    // the joint between me and my neighbor to the right.

    if (have_seam) {
        // Work my way down the seam, so long as all points
        // I inspect are closer to my seam than to a neighboring
        // seam.  Don't worry about whether edges are
        // Delaunay or not.  Note that {left,right}.{a,ai,ap} are
        // no longer needed.

        left.reinitialize(starter, regions[col]->rightmost, cw);
        right.reinitialize(starter, regions[neighbor]->leftmost, ccw);
        starter->set_tentative(true, starter);
        if (closest_seam(left.p) == col
                && closest_seam(right.p) == col) {
            my_tentative_edges.enqueue(starter, col);
            cur_edge = baste(left, right, ccw, col,
                             starter, &my_tentative_edges);
        } else {
            // access to starter must be synchronized
            regions[col]->tentative_edges->enqueue(starter, col);
            cur_edge = starter;
        }
        // Note that cur_edge is an sh_ptr, _not_ an un_ptr.
        // We are not privatizing it; we're just remembering it.
    }

    bar->wait("lower private baste");

    // Work down the disputed lower portion of the seam,
    // synchronizing with other threads, and quitting if one of them
    // boxes me in.

    if (have_seam) {
        synchronized_baste(left.p, right.p, ccw, col, cur_edge, regions);
    }

    bar->wait("lower synchronized baste");

    starter = starter_s;        // re-privatize

    // Stitch up the guaranteed-private upper part of
    // the joint between me and my neighbor to the right.

    if (have_seam) {
        left.p = starter->get_points(0, starter);
        right.p = starter->get_points(1, starter);
            // Those last two lines depend on the assumption that the
            // edge constructor makes its first argument points[0].
        left.update(starter, ccw);
        right.update(starter, cw);
        if (closest_seam(left.p) == col
                && closest_seam(right.p) == col) {
            cur_edge = baste(left, right, cw, col,
                             starter, &my_tentative_edges);
        } else {
            cur_edge = starter;
        }
    }

    bar->wait("upper private baste");

    // Work up the disputed upper portion of the seam,
    // synchronizing with other threads, and quitting if one of them
    // boxes me in.

    if (have_seam) {
        synchronized_baste(left.p, right.p, cw, col, cur_edge, regions);
    }

    bar->wait("upper synchronized baste");

    // Reconsider those edges that are guaranteed to be
    // in my geometric region (closest to my seam):

    {   edge::Up e;
        edge::Sp e1;  edge::Sp e2;
        edge::Sp e3;  edge::Sp e4;
        while ((e = my_tentative_edges.dequeue(col)) != 0) {
            if (!reconsider<edge::Up, edge::Up>(e, col, e1, e2, e3, e4)) {
                // have to defer to synchronized phase
                regions[col]->tentative_edges->enqueue(e, col);
            }
            if (e1 != 0) my_tentative_edges.enqueue(e1, col);
            if (e2 != 0) my_tentative_edges.enqueue(e2, col);
            if (e3 != 0) my_tentative_edges.enqueue(e3, col);
            if (e4 != 0) my_tentative_edges.enqueue(e4, col);
        }
    }

    bar->wait("private reconsideration");

    // Reconsider edges in disputed territory:

    {   edge::Sp e;
        while ((e = regions[col]->tentative_edges->dequeue(col)) != 0) {
            reconsider_edge(e, col, regions);
        }
        // try to help peers (simplistic work stealing):
        int i = (col + 1) % num_workers;
        while (i != col) {
            while ((e = regions[i]->tentative_edges->dequeue(col)) != 0) {
                reconsider_edge(e, col, regions);
                while ((e = regions[col]->tentative_edges->
                                                     dequeue(col)) != 0) {
                    reconsider_edge(e, col, regions);
                }
            }
            i = (i + 1) % num_workers;
        }
    }

    bar->wait("synchronized reconsideration");
}

void worker::operator()() {
    do_work(col, regions, bar);
}
