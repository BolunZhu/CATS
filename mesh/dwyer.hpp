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

/*  dwyer.h
 *
 *  Sequential solver
 */

#ifndef DWYER_H
#define DWYER_H

#include <vector>
#include "point.hpp"

// Recursively triangulate my_points[l..r].
// Dim0 values range from [low0..high0].
// Dim1 values range from [low1..high1].
//
// Base case when 1, 2, or 3 points.
//
// Using a slight variation on Dwyer's algorithm, we partition along
// whichever dimension appears to be the widest.  For simplicity, we
// use the range of coordinate values to estimate this, which will
// be fine for uniformly distributed points.  The purpose of the
// choice is to avoid creating long edges that are likely to be
// broken when stitching subproblems back together.  We partition
// along dimension 0; parity specifies whether this is X or Y.
//
extern void dwyer_solve(std::vector<point>& my_points, int l, int r,
                        int low0, int high0, int low1, int high1,
                        int parity);

#endif
