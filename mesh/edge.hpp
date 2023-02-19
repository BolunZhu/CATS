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

/*  edge.hpp
 *
 *  Edges encapsulate the bulk of the information about the triangulation.
 *  Each edge contains references to its endpoints and to the next
 *  edges clockwise and counterclockwise about those endpoints.
 */

#ifndef EDGE_H
#define EDGE_H

#include <iostream>
    using std::ostream;
    using std::cout;
    using std::cerr;

#include "queues.hpp"
#include "config.hpp"
#include "point.hpp"


class edge : public Object {
public:
    // Create Sp, Rp, Wp, Up types:
    DECLARE_SMART_POINTERS(edge)

private:
    // utility routine for constructor
    // can be instantiated with <edge::Wp> or with <edge::Up>
    template <template <class> class wrtp>
    static void initialize_end(point* p, wrtp<edge> e, int end, int dir,
                               wrtp<edge>& self_w);
public:
    GENERATE_ARRAY(point*, points, 2)
        // point* points[2];
    GENERATE_2DARRAY(edge::Sp, neighbors, 2, 2)
        // edge* neighbors[2][2];
        // indexed first by edge end and then by direction
    GENERATE_FIELD(bool, tentative)
        // Is this edge currently in a tentative_edges queue?


    // Default constructor, for use by create() and clone().
    //
    edge() { }

public:
    // Because tx_alloc puts newly-allocated space directly on the
    // DeleteOnAbort list, we cannot afford to suffer an abort in the
    // middle of a constructor.  We therefore use this pseudo-constructor
    // instead.
    // Connect points A and B, inserting dir (CW or CCW) of edge Ea at
    // the A end and 1-dir of edge Eb at the B end.  Either or both of Ea
    // and Eb may be null.  Sets reference parameter self to be
    // edge::Sp(new edge).  Can be instantiated with <edge::Wp> or with
    // <edge::Up>
    //
    template <template <class> class wrtp>
    static void create(point* a, point* b,
                       wrtp<edge> ea, wrtp<edge> eb, int dir,
                       sh_ptr<edge>& self);

    // print self in canonical form (leftmost point first)
    // called only when privatized
    //
    void print(ostream& os = cout) const {
        // only safe in non transactional code
        assert(stm::not_in_transaction());
        point* a = m_points[0];
        point* b = m_points[1];
        if (*b < *a) {
            point* t = a;  a = b;  b = t;
        }
        os << a->coord[xdim] << " " << a->coord[ydim] << " "
           << b->coord[xdim] << " " << b->coord[ydim] << "\n";
    }
};


template <template <class> class rdp>
inline static const bool not_equal(const rdp<edge>& lhs, const rdp<edge>& rhs)
{
    return ((lhs->get_points(0, lhs) != rhs->get_points(0, rhs)) ||
            (lhs->get_points(1, rhs) != rhs->get_points(1, rhs)));
}

// return index of point p within edge
//
template <template <class> class rdp>
    // can be instantiated with <edge::Rp> or <edge::Up>
int index_of(rdp<edge> e, const point* p) {
    if (e->get_points(0, e) == p) return 0;
    else {
        assert (e->get_points(1, e) == p);
        return 1;
    }
}

// Does the same thing as above. Added because it's more convenient
// for the code that I am writing to pass the rd_ptr by reference.
template <template <class> class rdp>
int index_of(const rdp<edge>& e, const point& p)
{
    if (*(e->get_points(0, e)) == p)
        return 0;
    else {
        assert (*(e->get_points(1, e)) == p);
        return 1;
    }
}

// Edge may not be Delaunay.  See if it's the diagonal of a convex
// quadrilateral.  If so, check whether it should be flipped.  If so,
// return (through ref params) the edges of the quadrilateral for future
// reconsideration.  In the private (nontransactinal) instantiation,
// do all of this only if all points I touch are closest to my seam:
// returns false iff called nontransactionally and edge cannot safely
// be reconsidered now.
//
template<class edgeRp, class edgeWp>
    // can be instantiated with
    // <edge::Rp, edge::Wp> or <edge::Up, edge::Up>.
bool reconsider(edgeWp self, const int seam,
                edge::Sp& e1, edge::Sp& e2, edge::Sp& e3, edge::Sp& e4);

#ifdef FGL
// alternative, FGL version: optimistic fine-grain locking.
void synchronized_reconsider(edge::Up self, const int seam,
                             simple_queue<edge::Sp> *tentative_edges);
#endif  // FGL

// Take self out of edges, point edge lists.
// Should only be called when flipping an edge, so destroyed
// edge should have neighbors at both ends.
// In FGL version, this was the destructor, and caller needed to
// hold locks covering endpoints and neighbors.  Can't be in the
// destructor in STM version, because this needs to happen immediately,
// and user doesn't control deletion time with delayed reclamation.
//
template <template <class> class wrtp> void destroy(wrtp<edge>& e);

// template specializations to drive use of private or transactional
// hash sets (among other things).
//
template <class T> inline bool txnal();
template <> inline bool txnal<edge::Wp>() { return true; }
template <> inline bool txnal<edge::Rp>() { return true; }
template <> inline bool txnal<edge::Up>() { return false; }

#endif
