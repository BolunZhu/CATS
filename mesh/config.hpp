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

/*  config.hpp
 *
 *  Global definitions.
 */

#ifndef MESH_CONFIG_H__
#define MESH_CONFIG_H__

#include <iostream>
    using std::cerr;
#include <assert.h>
#include <time.h>

#include <stm/stm.hpp>
    using namespace stm;

#include "macros.hpp"
#include "lock.hpp"

#ifdef __APPLE__
inline void* memalign(size_t alignment, size_t size)
{
    return malloc(size);
}
#else
#   include <malloc.h>     // some systems need this for memalign()
#endif


extern int num_points;              // number of points
extern int num_workers;             // threads
extern bool output_incremental;     // dump edges as we go along
extern bool output_end;             // dump edges at end
extern bool verbose;                // print stats
    // verbose <- (output_incremental || output_end)

#include "point.hpp"

static const int MAX_WORKERS = 32;

#define CACHELINESIZE 64        // needs to be a manifest constant

extern d_lock io_lock;
extern unsigned long long start_time;
extern unsigned long long last_time;
extern int num_points;

// This macro should be used inside the public portion of the
// declaration of transactional class TC.  It assumes that foo::Sp
// is nicer than stm::sh_ptr<foo>.
#define DECLARE_SMART_POINTERS(TC) \
    typedef stm::sh_ptr< TC > Sp;   /* opaque                 */ \
    typedef stm::rd_ptr< TC > Rp;   /* readable               */ \
    typedef stm::wr_ptr< TC > Wp;   /* readable & writable    */ \
    typedef stm::un_ptr< TC > Up;   /* unprotected (nontxnal) */

#endif // MESH_CONFIG_H__
