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

/*  mesh.cpp
 *
 *  Delaunay triangularization.
 */

#include <unistd.h>     // for getopt()
#include <iostream>
    using std::cout;
    using std::cerr;

#include "config.hpp"
#include "lock.hpp"
#include "point.hpp"
#include "edge.hpp"
#include "edge_set.hpp"
#include "my_thread.hpp"
#include "dwyer.hpp"
#include "worker.hpp"


d_lock io_lock;
unsigned long long start_time;
unsigned long long last_time;
edge_set *edges;

int num_points = 100;               // number of points
int num_workers = 4;                // threads
static int seed = 1;                // for random # gen

bool output_incremental = false;    // dump edges as we go along
bool output_end = false;            // dump edges at end
bool verbose = false;               // print stats
    // verbose <- (output_incremental || output_end)
static bool read_points = false;    // read points from stdin

static void usage() {
    cerr << "usage: mesh [-p] [-oi] [-oe] [-v] [-n points] [-w workers] [-s seed]\n";
    cerr << "\t-p: read points from stdin\n"
         << "\t-oi: output edges incrementally\n"
         << "\t-oe: output edges at end\n"
         << "\t-v: print timings, etc., even if not -oi or -oe\n";
    exit(1);
}

static void parse_args(int argc, char* argv[]) {
    int c;
    while ((c = getopt(argc, argv, "o:vpn:w:s:")) != -1) {
        switch (c) {
            case 'o':
                verbose = true;
                switch (optarg[0]) {
                    case 'i':
                        output_incremental = true;
                        break;
                    case 'e':
                        output_end = true;
                        break;
                    default:
                        usage();    // does not return
                }
                break;
            case 'p':
                read_points = true;
            case 'v':
                verbose = true;
                break;
            case 'n':
                num_points = atoi(optarg);
                break;
            case 'w':
                num_workers = atoi(optarg);
                if (num_workers < 1 || num_workers > MAX_WORKERS) {
                    cerr << "numWorkers must be between 1 and "
                         << MAX_WORKERS << "\n";
                    exit(1);
                }
                break;
            case 's':
                seed = atoi(optarg);
                assert (seed != 0);
                break;
            case '?':
            default:
                usage();    // does not return
        }
    }
    if (optind != argc) usage();    // does not return
}

int main(int argc, char* argv[]) {
    parse_args(argc, argv);
    stm::init(/*cm_type*/       "Polka",
              /*validation*/    "invis-eager",
              /*use_static_cm*/ true);

    if (verbose) {
        // Print args, X and Y ranges for benefit of optional display tool:
        for (int i = 0; i < argc; i++) {
            cout << argv[i] << " ";
        }
        cout << "\n";
    }
    create_points(read_points ? 0 : seed);

    start_time = last_time = getElapsedTime();

    edges = new edge_set;
        // has to be done _after_ initializing num_workers
        // But note: initialization will not be complete until every
        // thread calls help_initialize()

    if (num_workers == 1) {
        int min_coord = -(2 << (MAX_COORD_BITS-1));      // close enough
        int max_coord = (2 << (MAX_COORD_BITS-1)) - 1;   // close enough
        edges->help_initialize(0);
        unsigned long long now;
        if (verbose) {
            now = getElapsedTime();
            cout << "time: " << (now - start_time)/1e9 << " "
                             << (now - last_time)/1e9
                             << " (point partitioning)\n";
            last_time = now;
        }
        dwyer_solve(all_points, 0, num_points - 1,
                    min_coord, max_coord, min_coord, max_coord, 0);
        now = getElapsedTime();
        if (verbose) {
            cout << "time: " << (now - start_time)/1e9 << " "
                             << (now - last_time)/1e9
                             << " (Dwyer triangulation)\n";
        }
        cout << "time: " << (now - start_time)/1e9 << " "
                         << (now - last_time)/1e9
                         << " (join)\n";
        last_time = now;
    } else {
        region_info **regions = new region_info*[num_workers];
        barrier *bar = new barrier(num_workers);

        thread **workers = new thread*[num_workers];
        for (int i = 0; i < num_workers; i++) {
            workers[i] = new thread(new worker(i, regions, bar));
        }
        for (int i = 0; i < num_workers; i++) {
            delete workers[i];      // join
        }
        unsigned long long now = getElapsedTime();
        cout << "time: " << (now - start_time)/1e9 << " "
                         << (now - last_time)/1e9
                         << " (join)\n";
        last_time = now;
    }

    if (output_end) edges->print_all();

    // edges->print_stats();
}
