///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2005, 2006, 2007, 2008, 2009
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

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <cstdlib>
#include <string>
#include <iostream>

#include "Benchmark.hpp"
#include "Counter.hpp"
#include "FGL.hpp"
#include "CGHash.hpp"
#include "Hash.hpp"
#include "LFUCache.hpp"
#include "LinkedList.hpp"
#include "LinkedListRelease.hpp"
#include "PrivList.hpp"
#include "RBTree.hpp"
#include "RBTreeLarge.hpp"
#include "RandomGraphList.hpp"
#include "TypeTest.hpp"
#include "VerifyNesting.hpp"
#include "VerifyRetry.hpp"
#include "DList.hpp"
#include "WWPathology.hpp"
#include "RWPathology.hpp"
#include "StridePathology.hpp"
#include "ListOverwriter.hpp"
#include "PrivTree.hpp"
#include "Forest.hpp"

using namespace bench;

#ifdef _MSC_VER
#include "../alt-license/XGetOpt.h"
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::string;

BenchmarkConfig BMCONFIG;

static void usage()
{
    cerr << "Usage: Bench -B benchmark -C contention manager ";
    cerr << "-V validation strategy [flags]" << endl;
    cerr << "  Benchmarks:" << endl;
    cerr << "    Counter            Shared counter" << endl;
    cerr << "    LinkedList         Sorted linked list" << endl;
    cerr << "    LinkedListRelease  LinkedList, early release" << endl;
    cerr << "    DList              Sorted doubly linked list" << endl;
    cerr << "    WWPathology        Pathological write-write conflicts" << endl;
    cerr << "    RWPathology        Pathological read-write conflicts" << endl;
    cerr << "    StridePathology    Another attempt to cause livelock" << endl;
    cerr << "    HashTable          256-bucket hash table" << endl;
    cerr << "    RBTree (default)   Red-black tree" << endl;
    cerr << "    RBTreeLarge        Red-black tree with 4KB nodes" << endl;
    cerr << "    LFUCache           Web cache simulation" << endl;
    cerr << "    RandomGraph        Graph with 4 connections per node" << endl;
    cerr << "    PrivList           Linked list privatization test" << endl;
    cerr << "    PrivTree           RBTree privatization test" << endl;
    cerr << "    TypeTest           Test that word-based STMs handle types correctly" << endl;
    cerr << "    VerifyRetry        Test that retry works" << endl;
    cerr << "    VerifyNesting      Simple test that subsumption nesting works" << endl;
    cerr << "    FineGrainList      Lock-based sorted linked list" << endl;
    cerr << "    CoarseGrainHash    256-bucket hash table, per-node locks"
         << endl;
    cerr << "    FineGrainHash      256-bucket hash table, per-bucket locks"
         << endl;
    cerr << endl;
    cerr << "  RSTM Contention Managers:" << endl;
    cerr << "     Polka (default), Eruption, Highlander, Karma, Killblocked, ";
    cerr << "Polite" << endl;
    cerr << "     Polkaruption, Timestamp, Aggressive, Whpolka, Greedy, ";
    cerr << "Polkavis" << endl;
    cerr << "     Gladiator, Justice, PoliteR, Reincarnate, Serializer, ";
    cerr << "AggressiveR" << endl;
    cerr << "     Timid, TimidR" << endl;
    cerr << "  ET Contention Managers:" << endl;
    cerr << "     Polite, Aggressive, Timid, Patient" << endl;
    cerr << endl;
    cerr << "  Validation Strategies:" << endl;
    cerr << "     RSTM:      invis-eager (default), invis-lazy, vis-eager, vis-lazy" << endl;
    cerr << "     Redo_Lock: invis-eager (default), invis-lazy, vis-eager, vis-lazy" << endl;
    cerr << "     et:        ee, el, ll";
    cerr << endl << endl;
    cerr << "  Flags:" << endl;
    cerr << "    -d: number of seconds to time (default 5)" << endl;
    cerr << "    -m: number of distinct elements (default 256)" << endl;
    cerr << "    -p: number of threads (default 2)" << endl;
    cerr << endl;
    cerr << "  Other:" << endl;
    cerr << "    -h: print help (this message)" << endl;
    cerr << "    -q: quiet mode" << endl;
    cerr << "    -v: verbose mode" << endl;
    cerr << "    -c: verbose, and output csv" << endl;
    cerr << "    -Z: toggle verification at end of benchmark" << endl;
    cerr << "    -R: % read-only txns (1/2 of remainder inserts, removes)"
         << endl;
    cerr << "    -T:[lh] perform light or heavy unit testing" << endl;
    cerr << "    -X: execute fixed transaction count, not for a duration"
         << endl;
    cerr << "    -W: warm up the data structure with half of the elements"
         << endl;
    cerr << "    -I: thread0 executes all transactions inevitably" << endl;
    cerr << "    -P: threads execute with priority" << endl;
    cerr << endl;
}

int main(int argc, char** argv)
{
    int opt;

    // parse the command-line options
    while ((opt = getopt(argc, argv, "B:C:d:m:p:hqvZxV:R:T:WX:IcP")) != -1) {
        switch(opt) {
          case 'B':
            BMCONFIG.bm_name = string(optarg);
            break;
          case 'V':
            BMCONFIG.stm_validation = string(optarg);
            break;
          case 'T':
            BMCONFIG.unit_testing = string(optarg)[0];
            break;
          case 'C':
            BMCONFIG.cm_type = string(optarg);
            BMCONFIG.use_static_cm = false;
            break;
          case 'W':
            BMCONFIG.doWarmup = true;
            break;
          case 'X':
            BMCONFIG.execute = atoi(optarg);
            break;
          case 'd':
            BMCONFIG.duration = atoi(optarg);
            break;
          case 'm':
            BMCONFIG.datasetsize = atoi(optarg);
            break;
          case 'p':
            BMCONFIG.threads = atoi(optarg);
            break;
          case 'h':
            usage();
            return 0;
          case 'q':
            BMCONFIG.verbosity = 0;
            break;
          case 'v':
            BMCONFIG.verbosity = 2;
            break;
          case 'c':
            BMCONFIG.verbosity = 3;
            break;
          case 'Z':
            BMCONFIG.verify = !BMCONFIG.verify;
            break;
          case 'R':
            BMCONFIG.lookupPct = atoi(optarg);
            BMCONFIG.insertPct = (100 - BMCONFIG.lookupPct)/2 + atoi(optarg);
            break;
          case 'I':
            BMCONFIG.inev = true;
            break;
          case 'P':
            BMCONFIG.priority = true;
            break;
        }
    }

    // make sure that the parameters all make sense
    BMCONFIG.verifyParameters();

    // initialize stm so that we have transactional mm, then verify the
    // benchmark parameter and construct the benchmark object
    stm::init(BMCONFIG.cm_type, BMCONFIG.stm_validation,
              BMCONFIG.use_static_cm);

    Benchmark* B = NULL;

    if (BMCONFIG.bm_name == "Counter")
        B = new CounterBench();
    else if (BMCONFIG.bm_name == "FineGrainList")
        B = new IntSetBench(new FGL(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "FineGrainHash")
        B = new IntSetBench(new bench::HashTable<FGL>(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "CoarseGrainHash")
        B = new IntSetBench(new CGHash(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RandomGraph")
        B = new RGBench(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "HashTable")
        B = new IntSetBench(new bench::Hash(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "LFUCache")
        B = new LFUTest();
    else if (BMCONFIG.bm_name == "LinkedList")
        B = new IntSetBench(new LinkedList(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "LinkedListRelease")
        B = new IntSetBench(new LinkedListRelease(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "DList")
        B = new IntSetBench(new DList(BMCONFIG.datasetsize),
                            BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "PrivList")
        B = new PrivList(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "TypeTest")
        B = new TypeTest(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "VerifyRetry")
        B = new VerifyRetry(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "VerifyNesting")
        B = new VerifyNesting(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RBTree")
        B = new IntSetBench(new RBTree(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RBTreeLarge")
        B = new IntSetBench(new RBTreeLarge(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "WWPathology")
        B = new WWPathology(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name.substr(0,11) == "StridePathology") {
        string str = BMCONFIG.bm_name;
        int pos1 = str.find_first_of('-', 0);
        int pos2 = str.size();
        int size = atoi(str.substr(pos1+1, pos2-pos1-1).c_str());
        B = new StridePathology(BMCONFIG.datasetsize, size);
    }
    else if (BMCONFIG.bm_name.substr(0,6) == "Forest") {
        string str = BMCONFIG.bm_name;
        int pos1 = str.find_first_of('-', 0);
        int pos2 = str.size();
        string configstr = str.substr(pos1+1, pos2-pos1-1).c_str();
        B = new Forest(configstr);
    }
    else if (BMCONFIG.bm_name.substr(0,14) == "RWPathology") {
        string str = BMCONFIG.bm_name;
        int pos1 = str.find_first_of('-', 0);
        int pos2 = str.size();
        int size = atoi(str.substr(pos1+1, pos2-pos1-1).c_str());
        B = new RWPathology(BMCONFIG.datasetsize, size);
    }
    else if (BMCONFIG.bm_name.substr(0,8) == "PrivTree") {
        string str = BMCONFIG.bm_name;
        int pos1 = str.find_first_of('-', 0);
        int pos2 = str.size();
        int size = atoi(str.substr(pos1+1, pos2-pos1-1).c_str());
        B = new PrivTree(BMCONFIG.datasetsize, size);
    }
    else if (BMCONFIG.bm_name == "RBTree256") {
        BMCONFIG.datasetsize = 256;
        BMCONFIG.doWarmup = true;
        B = new IntSetBench(new RBTree(), BMCONFIG.datasetsize);
    }
    else if (BMCONFIG.bm_name == "RBTree1K") {
        BMCONFIG.datasetsize = 1024;
        BMCONFIG.doWarmup = true;
        B = new IntSetBench(new RBTree(), BMCONFIG.datasetsize);
    }
    else if (BMCONFIG.bm_name == "RBTree64K") {
        BMCONFIG.datasetsize = 65536;
        BMCONFIG.doWarmup = true;
        B = new IntSetBench(new RBTree(), BMCONFIG.datasetsize);
    }
    else if (BMCONFIG.bm_name == "RBTree1M") {
        BMCONFIG.datasetsize = 1048576;
        BMCONFIG.doWarmup = true;
        B = new IntSetBench(new RBTree(), BMCONFIG.datasetsize);
    }
    else if (BMCONFIG.bm_name == "ListOverwriter")
        B = new ListOverwriter(BMCONFIG.datasetsize);
    else
        argError("Unrecognized benchmark name " + BMCONFIG.bm_name);

    // print the configuration for this run of the benchmark
    BMCONFIG.printConfig();

    // either verify the data structure or run a timing experiment
    if (BMCONFIG.unit_testing != ' ') {
        if (B->verify(BMCONFIG.unit_testing == 'l' ? LIGHT : HEAVY))
            cout << "Verification succeeded" << endl;
        else
            cout << "Verification failed" << endl;
    }
    else {
        B->measure_speed();
    }
}
