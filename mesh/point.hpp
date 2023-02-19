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

/*  point.hpp
 *
 *  Points are immutable; their coordinates never change.
 *  Ideally they would point to one adjacent edge, but that would
 *  make them mutable, and then they'd have to be transactional.
 *  The first_edges array is a work-around for that.
 */

#ifndef POINT_H
#define POINT_H

#include <set>
    using std::set;
#include <vector>
#include "config.hpp"

class edge;

static const int xdim = 0;
static const int ydim = 1;
static const int ccw = 0;       // counter-clockwise
static const int cw = 1;        // clockwise

static const int MAX_COORD_BITS = 24;
    // coordinate values in the range -2^24..(2^24-1)
    // permit bucket calculations without overflow, so long as
    // there are no more than 32 threads

class edge_ref : public Object {
    virtual edge_ref* clone() const {
        edge_ref* dup = new edge_ref();
        dup->m_e = m_e;
        return dup;
    }
#ifdef NEED_REDO_METHOD
    virtual void redo(SharedBase* l_sh) {
        edge_ref* l = static_cast<edge_ref*>(l_sh);
        m_e = l->m_e;
    }
#endif
public:
    GENERATE_FIELD(sh_ptr<edge>, e);
};

class point;
extern std::vector<point> all_points;
extern std::vector<sh_ptr<edge_ref> > first_edges;

class point
{
    //      Points are a funny beast. They have been split into two parts,
    //      mutable and immutable pieces. These are stored in two separate
    //      vectors. The index of a point within the all_points vector
    //      corresponds to the index of its first edge in the first_edges
    //      vector.
    //
    //      This is fine, but the implementation of the algorithm to find
    //      the index of a point in the all_points array means that a point's
    //      edge cannot be accessed if the point isn't in the all_points array
    //      (see getEdgeRef()).
    //
    //      In order to prevent bugs, we disable copying and assignment for
    //      points. The only problem with this is that the vector of points
    //      needs to be able to copy points around internally. These friend
    //      declarations give a vector of points the ok to copy points. As long
    //      as the vector is the all_points vector everything works.
    //
    //      Ultimately this finds potential bugs at compile time.
    //
    friend class std::vector<point>;
    friend void __gnu_cxx::new_allocator<point>::construct(point*,
                                                           const point&);
    friend void std::_Construct<point, point>(point*, const point&);

    // Initial point creation needs to be allowed to copy points.
    //
    friend void create_points(const int);

    // Swapping two points in a vector requires private point access.
    //
    friend void swap_points(std::vector<point>&, const int, const int);

    //      I really don't know why these three are needed. They just forward
    //      to the vector's member function, which should be covered by the
    //      friend class std::vector<point> specification... oh well. This
    //      allows calls to swap and resize.
    friend void swap(std::vector<point>&, std::vector<point>&);
    friend void resize(std::vector<point>&, const size_t);

#ifdef FGL
    d_lock l;
#endif
    inline sh_ptr<edge_ref>& getEdgeRef() const {
        const int i = this - &(*all_points.begin());
        assert((i >= 0) && (i < num_points));   // just in case someone tries
                                                // to use this when not in the
                                                // all_points array.
        return first_edges[i];
    }

public:
    int coord[2];       // final; no accessors needed

    enum Direction {
        CCW = ccw,
        CW = cw
    };

    // The following routines save space by assuming that all points
    // lie in global array all_points and all edge_refs lie in global
    // array first_refs.  They also segregate the mutable first_edge
    // reference from the rest of a point, leaving the rest immutable
    // and therefore safely accessible without synchronization.
    //
    template <template <class> class rdp>
    sh_ptr<edge> get_first_edge() const {
        rdp<edge_ref> e(getEdgeRef());
        return e->get_e(e);
    }

    template <template <class> class wrtp>
    void set_first_edge(sh_ptr<edge> fe) {
        wrtp<edge_ref> e(getEdgeRef());
        e->set_e(fe, e);
    }

    size_t hash() const
    {
        return coord[xdim] ^ coord[ydim];
    }

    bool operator==(const point &o) const
    {
        return coord[xdim] == o.coord[xdim]
            && coord[ydim] == o.coord[ydim];
    }

    bool operator!=(const point &rhs) const
    {
        return !(*this == rhs);
    }

    // For locking and printing in canonical (sorted) order.
    //
    bool operator<(const point& o) const {
        return (coord[xdim] < o.coord[xdim]
                || (coord[xdim] == o.coord[xdim]
                    && coord[ydim] < o.coord[ydim]));
    }

#ifdef FGL
    void acquire() {
        l.acquire();
    }
    void release() {
        l.release();
    }
#endif  // FGL

  private:
    // Protects me from accidentally creating a point.
    point() { }

    //      Does not copy the lock in FGL. Not sure at this point if it should.
    //      This is private so that only friends can pass points by value. Only
    //      acutally used in create_points.
    point(const point& p) {
        coord[xdim] = p.coord[xdim];
        coord[ydim] = p.coord[ydim];
    }
};

struct eq_point {
    bool operator()(const point* const p1, const point* const p2) const {
        return *p1 == *p2;
    }
};

struct lt_point {
    bool operator()(const point* const p1, const point* const p2) const {
        return *p1 < *p2;
    }
};

struct hash_point {
    size_t operator()(const point* const p) const {
        return p->hash();
    }
};

#ifdef FGL
// lock all points in given set at beginning of scope, release at end
//
class with_locked_points {
    set<point*, lt_point> *S;
public:
    with_locked_points(set<point*, lt_point> &pts) {
        S = &pts;
        for (set<point*, lt_point>::iterator it = S->begin();
                it != S->end(); ++it) {
            (*it)->acquire();
        }
    }
    ~with_locked_points() {
        for (set<point*, lt_point>::reverse_iterator it = S->rbegin();
                it != S->rend(); ++it) {
            (*it)->release();
        }
    }
};

// To facilitate constructions of the form
//     point_set S;
//     with_locked_points lp(S | p1 | p2 | ... | pN);
//
class point_set : public set<point*, lt_point> {
public:
    point_set &operator|(point *e) {
        insert(e);
        return *this;
    }
};
#endif  // FGL

// these are set by create_points().
//
extern int min_coord[];
extern int max_coord[];

// The point space is divided into vertical "buckets" -- twice as many as
// there are worker threads.  During initial Dwyer triangulation, thread
// zero works, privately, on buckets 0 and 1; thread 1 on buckets 2 and 3;
// etc.  During private basting, each thread takes a _seam_ and works on
// the buckets closest to it: thread 0 on buckets 0, 1, and 2; thread 1
// on buckets 3 and 4; ...; thread n-1 on buckets 2n-3, 2n-2, and 2n-1.
//
// Routines bucket, stripe, and closest_seam manage this division of labor.
// To avoid integer overflow, they count on coordinate values spanning less
// than 2^26.
//
// Global set edges contains a separate hash table for each bucket, ensuring
// private access during each private phase of computation.
//
inline int bucket(const point* const p) {
    return ((p->coord[xdim] - min_coord[xdim]) * num_workers * 2)
            / (max_coord[xdim] - min_coord[xdim] + 1);
}
inline int stripe(const point* const p) {
    return bucket(p) / 2;
}
inline int closest_seam(const point* const p) {
    const int b = bucket(p);
    if (b == 0) return 0;
    if (b == (num_workers * 2 - 1)) return num_workers - 2;
    return (b-1)/2;
}

// Does circumcircle of A, B, C (ccw) contain D?
//
bool encircled(const point* A, const point* B,
               const point* C, const point* D, const int dir);

// Is angle from p1 to p2 to p3, in direction dir
// around p2, greater than or equal to 180 degrees?
//
bool extern_angle(const point* p1, const point* p2,
                  const point* p3, const int dir);

// Create all points.  Read from stdin if seed == 0.
//
extern void create_points(const int seed);

// Swaps two points in the vector. Used to allow point copying to be protected.
//
inline void swap_points(std::vector<point>& points, const int i, const int j)
{
    point t = points[i];
    points[i] = points[j];
    points[j] = t;
}

// Swaps all of the points between the two vectors. Used to allow point
// copying to be protected.
inline void swap(std::vector<point>& a, std::vector<point>& b)
{
    a.swap(b);
}

// Resizing a vector allows private access to the point's copy capability.
inline void resize(std::vector<point>& v, const size_t size)
{
    v.resize(size);
}

#endif
