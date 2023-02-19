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

/*  point.cpp
 *
 *  Points are immutable; their coordinates never change.
 *  Ideally they would point to one adjacent edge, but that would
 *  make them mutable, and then they'd have to be transactional.
 *  The first_edges array is a work-around for that.
 */

#include <limits.h>     // for INT_MIN, INT_MAX
#include <stdlib.h>     // for random()
#include <ext/hash_set>
    using __gnu_cxx::hash_set;
#include <iostream>
    using std::cout;
    using std::cerr;
#include <iomanip>
    using std::flush;
#include <vector>
 using std::vector;
#include "config.hpp"
#include "point.hpp"

vector<point> all_points;
vector<sh_ptr<edge_ref> > first_edges;

int min_coord[2] = {INT_MAX, INT_MAX};
int max_coord[2] = {INT_MIN, INT_MIN};

// 3x3 determinant
//
static double det3(const double a, const double b, const double c,
                   const double d, const double e, const double f,
                   const double g, const double h, const double i) {
    return a * (e*i - f*h)
         - b * (d*i - f*g)
         + c * (d*h - e*g);
}

// 4x4 determinant
//
static double det4(
        const double a, const double b, const double c, const double d,
        const double e, const double f, const double g, const double h,
        const double i, const double j, const double k, const double l,
        const double m, const double n, const double o, const double p) {
    return a * det3(f, g, h, j, k, l, n, o, p)
         - b * det3(e, g, h, i, k, l, m, o, p)
         + c * det3(e, f, h, i, j, l, m, n, p)
         - d * det3(e, f, g, i, j, k, m, n, o);
}

// If A, B, and C are on a circle, in counter-clockwise order, then
// D lies within that circle iff the following determinant is positive:
//
// | Ax  Ay  Ax^2+Ay^2  1 |
// | Bx  By  Bx^2+By^2  1 |
// | Cx  Cy  Cx^2+Cy^2  1 |
// | Dx  Dy  Dx^2+Dy^2  1 |
//
bool encircled(const point* A, const point* B,
               const point* C, const point* D, const int dir) {
    if (dir == cw) {
        const point* t = A;  A = C;  C = t;
    }
    double Ax = A->coord[xdim];   double Ay = A->coord[ydim];
    double Bx = B->coord[xdim];   double By = B->coord[ydim];
    double Cx = C->coord[xdim];   double Cy = C->coord[ydim];
    double Dx = D->coord[xdim];   double Dy = D->coord[ydim];

    return det4(Ax, Ay, (Ax*Ax + Ay*Ay), 1,
                Bx, By, (Bx*Bx + By*By), 1,
                Cx, Cy, (Cx*Cx + Cy*Cy), 1,
                Dx, Dy, (Dx*Dx + Dy*Dy), 1) > 0;
}

// Is angle from p1 to p2 to p3, in direction dir
// around p2, greater than or equal to 180 degrees?
//
bool extern_angle(const point* p1, const point* p2,
                  const point* p3, const int dir) {
    if (dir == cw) {
        const point* t = p1;  p1 = p3;  p3 = t;
    }
    int x1 = p1->coord[xdim];     int y1 = p1->coord[ydim];
    int x2 = p2->coord[xdim];     int y2 = p2->coord[ydim];
    int x3 = p3->coord[xdim];     int y3 = p3->coord[ydim];

    if (x1 == x2) {                     // first segment vertical
        if (y1 > y2) {                  // points down
            return (x3 >= x2);
        } else {                        // points up
            return (x3 <= x2);
        }
    } else {
        double m = (((double) y2) - y1) / (((double) x2) - x1);
            // slope of first segment
        if (x1 > x2) {      // points left
            return (y3 <= m * (((double) x3) - x1) + y1);
            // p3 below line
        } else {            // points right
            return (y3 >= m * (((double) x3) - x1) + y1);
            // p3 above line
        }
    }
}

// Create all points.  Read from stdin if seed == 0.
//
void create_points(const int seed)
{
    for (int i = 0; i < num_points; i++) {
        sh_ptr<edge_ref> er(new edge_ref());
        first_edges.push_back(er);

        all_points.push_back(point());  // Do this here so points don't move
                                        // around. This is used during point
                                        // initialization, where we use a
                                        // hashtable of point* to make sure
                                        // there are no duplicates.
    }

    hash_set<point*, hash_point, eq_point> point_hash;

    srandom(seed);

    for (int i = 0; i < num_points; i++) {
        point& p = all_points[i];

        if (seed == 0) {
            int x;  scanf("%d", &x);
            int y;  scanf("%d", &y);
            assert(x >= -(2 << (MAX_COORD_BITS-1))
                        && x <= ((2 << (MAX_COORD_BITS-1)) - 1));
            assert(y >= -(2 << (MAX_COORD_BITS-1))
                        && y <= ((2 << (MAX_COORD_BITS-1)) - 1));
            p.coord[xdim] = x;
            p.coord[ydim] = y;
            assert(point_hash.find(&p) == point_hash.end());
                // no duplicates allowed
        } else {
            do {
                p.coord[xdim] = (random() &
                    ((2 << (MAX_COORD_BITS))-1)) - (2 << (MAX_COORD_BITS-1));
                p.coord[ydim] = (random() &
                    ((2 << (MAX_COORD_BITS))-1)) - (2 << (MAX_COORD_BITS-1));
            } while (point_hash.find(&p) != point_hash.end());
        }

        point_hash.insert(&p);
        if (p.coord[xdim] < min_coord[xdim]) min_coord[xdim] = p.coord[xdim];
        if (p.coord[ydim] < min_coord[ydim]) min_coord[ydim] = p.coord[ydim];
        if (p.coord[xdim] > max_coord[xdim]) max_coord[xdim] = p.coord[xdim];
        if (p.coord[ydim] > max_coord[ydim]) max_coord[ydim] = p.coord[ydim];
    }

    // Print point ranges for benefit of optional display tool:
    if (output_incremental || output_end) {
        cout << min_coord[xdim] << " " << max_coord[xdim] << " "
             << min_coord[ydim] << " " << max_coord[ydim] << flush << "\n";
    }
}
