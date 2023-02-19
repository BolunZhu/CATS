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
#include <pthread.h>
#else
#include <windows.h>
#include <time.h>
#endif

#ifdef _MSC_VER
#include <alt-license/rand_r.h>
#endif

#include <iostream>
#include <cassert>
#include <signal.h>
#include <vector>

#ifdef SPARC
#include <sys/types.h>
#endif

#include "../stm/support/atomic_ops.h"
#include "../stm/support/hrtime.h"
#include "Benchmark.hpp"

using std::cout;
using std::endl;

/**
 * set up a few globals for tracking the start and end time of the experiment
 */
#ifdef _MSC_VER
long globalEndTime = 0;
long starttime     = 0;
long endtime       = 0;
#else
unsigned long long starttime = 0;
unsigned long long endtime   = 0;
#endif

volatile bool ExperimentInProgress __attribute__ ((aligned(64))) = true;
std::string TxnOpDesc[TXN_NUM_OPS] =
    {"      ID", "Total   ", "Insert  ", "Remove  ", "Lookup T", "Lookup F"};

// signal handler to end the test
static void catch_SIGALRM(int sig_num)
{
    ExperimentInProgress = false;
    bench::shutdown_benchmark();
}

void barrier(int id, unsigned long nthreads)
{
    static struct
    {
        volatile unsigned long count;
        volatile unsigned long sense;
        volatile unsigned long thread_sense[256]; // hard-coded max threads
    } __attribute__ ((aligned(64))) bar = {0};

    bar.thread_sense[id] = !bar.thread_sense[id];
    if (fai(&bar.count) == nthreads - 1) {
        bar.count = 0;
        bar.sense = !bar.sense;
    }
    else
        while (bar.sense != bar.thread_sense[id]);     // spin
}

#ifdef _MSC_VER
static DWORD WINAPI run_benchmark(LPVOID arg)
#else
static void* run_benchmark(void* arg)
#endif
{
    thread_args_t* args    = (thread_args_t*)arg;
    int            id      = args->id;
    unsigned int   seed    = id;

    // if not thread0, create a transactional context
    if (id != 0)
        stm::init(BMCONFIG.cm_type, BMCONFIG.stm_validation,
                  BMCONFIG.use_static_cm);

    Benchmark* b = args->b;

    // do warmup in thread 0.  warmup inserts half of the elements in the
    // datasetsize
    if ((id == 0) && (BMCONFIG.doWarmup)) {
        for (int w = 0; w < BMCONFIG.datasetsize; w+=2) {
            b->random_transaction(args, &seed, w, BMCONFIG.lookupPct + 1);
        }
    }

    // everyone waits here, and then we start timing
    barrier(args->id, BMCONFIG.threads);

#if _MSC_VER
    if (id == 0) {
        starttime = clock();
        globalEndTime = starttime + (BMCONFIG.duration * 1000);
    }
#else
    if (id == 0) {
        // set the signal handler and kick off a timer unless execute is nonzero
        if (BMCONFIG.execute == 0) {
            signal(SIGALRM, catch_SIGALRM);
            alarm(BMCONFIG.duration);
        }

        // get the start time of the benchmark run
        starttime = getElapsedTime();
    }
#endif

    // everyone waits here, and then we run the experiment
    barrier(args->id, BMCONFIG.threads);

    if (BMCONFIG.execute > 0) {
        for (int e = 0; e < BMCONFIG.execute; e++) {
            int val = rand_r(&seed);
            int action = rand_r(&seed)%100;
            if (id == 0 && BMCONFIG.inev) {
                BEGIN_TRANSACTION;
                stm::try_inevitable();
                b->random_transaction(args, &seed, val, action);
                END_TRANSACTION;
            }
#if defined(STM_LIB_FAIR)
            else if (BMCONFIG.priority) {
                BEGIN_TRANSACTION;
                stm::setPrio(id);
                b->random_transaction(args, &seed, val, action);
                END_TRANSACTION;
            }
#endif
            else {
                b->random_transaction(args, &seed, val, action);
            }
            ++args->count[TXN_GENERIC];
        }
    }
    else {
#if _MSC_VER
        do {
            int val = rand_r(&seed);
            int action = rand_r(&seed)%100;
            b->random_transaction(args, &seed, val, action);
            ++args->count[TXN_GENERIC];
        } while (clock() < globalEndTime);
#else
        do {
            int val = rand_r(&seed);
            int action = rand_r(&seed)%100;
            // use inevitability if -I is on
            if (id == 0 && BMCONFIG.inev) {
                BEGIN_TRANSACTION;
                stm::try_inevitable();
                b->random_transaction(args, &seed, val, action);
                END_TRANSACTION;
            }
#if defined(STM_LIB_FAIR)
            else if (BMCONFIG.priority) {
                BEGIN_TRANSACTION;
                stm::setPrio(id);
                b->random_transaction(args, &seed, val, action);
                END_TRANSACTION;
            }
#endif
            else {
                b->random_transaction(args, &seed, val, action);
            }
            ++args->count[TXN_GENERIC];
        } while (ExperimentInProgress);
#endif
    }

    // in benchmarks that use retry(), there may be transactions that need to
    // be woken up.  wake them here via a call to wake_retriers()
    //
    // NB: exactly one thread should call the wake_retriers method, but we
    // can't be sure that thread0 will be awake.  However, any reasonably
    // correct benchmark should have at least one thread not in retry() at any
    // given time
    static volatile unsigned long mtx = 0;
    if (bool_cas(&mtx, 0, 1)) {
#ifdef STM_LIB_FAIR
        stm::halt_retry();
#else
        b->wake_retriers();
#endif
    }

    // now wait for everyone to wake up
    barrier(args->id, BMCONFIG.threads);

    // OK, everyone is done, so we can get the endtime here
#if _MSC_VER
    endtime = clock();
#else
    endtime = getElapsedTime();
#endif

    // Shut down STM for all threads other than thread 0, which may need to run
    // a sanity check
    if (args->id != 0)
        stm::shutdown(args->id);

    return 0;
}

void Benchmark::measure_speed()
{
    std::vector<thread_args_t> args;
    args.resize(BMCONFIG.threads);

#ifdef _MSC_VER
    std::vector<HANDLE> tid;
#else
    std::vector<pthread_t> tid;
#endif
    tid.resize(BMCONFIG.threads);

    // set up configuration structs for the threads we'll create
#ifndef _MSC_VER
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#endif
    for (int i = 0; i < BMCONFIG.threads; i++) {
        args[i].id = i;
        args[i].b = this;
        for (int op = 0; op < TXN_NUM_OPS; op++)
            args[i].count[op] = 0;
    }

    // actually create the threads
    for (int j = 1; j < BMCONFIG.threads; j++) {
#ifdef _MSC_VER
        tid[j] = CreateThread(NULL, 0, &run_benchmark, &args[j], 0, NULL);
#else
        pthread_create(&tid[j], &attr, &run_benchmark, &args[j]);
#endif
    }

    // all of the other threads should be queued up, waiting to run the
    // benchmark, but they can't until this thread starts the benchmark too...
    run_benchmark((void*)(&args[0]));

    // everyone should be done.  Join all threads so we don't leave anything
    // hanging around
    for (int k = 1; k < BMCONFIG.threads; k++) {
#ifdef _MSC_VER
        WaitForSingleObject(tid[k], INFINITE);
        CloseHandle(tid[k]);
#else
        pthread_join(tid[k], NULL);
#endif
    }

    // Run the sanity check
    if (BMCONFIG.verify) {
        bool sanity = (args[0].b)->sanity_check();
        if (BMCONFIG.verbosity > 0) {
            if (sanity)
                cout << "Completed sanity check." << endl;
            else
                cout << "Sanity check failed." << endl;
        }
    }

    // shut off transactions in thread 0
    stm::shutdown(0);

    // Count the transactions completed by all threads
    long Total_count = 0;
    for (int l = 0; l < BMCONFIG.threads; l++) {
        Total_count += args[l].count[TXN_GENERIC];
    }

    // raw output:  total tx, total time
    cout << "Transactions: " << Total_count
         << ",  time: " << endtime - starttime << endl;

    // prettier output:  total tx/time
    if (BMCONFIG.verbosity > 0) {
#if _MSC_VER
        cout << 1000 * Total_count / (endtime - starttime)
             << " Transactions per second" << endl;
#else
        cout << (1000000000LL * Total_count) / (endtime - starttime)
             << " txns per second (whole system)" << endl;
#endif
    }

    // even prettier output:  print the number of each type of tx
    if (BMCONFIG.verbosity > 1) {
        for (int op = 0; op < TXN_NUM_OPS; op++) {
            cout << TxnOpDesc[op];
            for (int l = 0; l < BMCONFIG.threads; l++) {
                cout << " | ";
                cout.width(7);
                if (op == TXN_ID)
                    cout << l;

                else cout << args[l].count[op];
                cout.width(0);
            }
            cout << " |" << endl;
        }
    }

    if (BMCONFIG.verbosity > 2) {
        // csv output
        cout << "csv"
             << ", name=" << BMCONFIG.bm_name
             << ", duration=" << BMCONFIG.duration
             << ", datasetsize=" << BMCONFIG.datasetsize
             << ", threads=" << BMCONFIG.threads
             << ", verbosity=" << BMCONFIG.verbosity
             << ", verify=" << BMCONFIG.verify
             << ", cm_type=" << BMCONFIG.cm_type
             << ", bm_name=" << BMCONFIG.bm_name
             << ", stm_validation=" << BMCONFIG.stm_validation
             << ", use_static_cm=" << BMCONFIG.use_static_cm
             << ", lookupPct=" << BMCONFIG.lookupPct
             << ", insertPct=" << BMCONFIG.insertPct
             << ", doWarmup=" << BMCONFIG.doWarmup
             << ", execute=" << BMCONFIG.execute
             << ", inev=" << BMCONFIG.inev
             << ", txns=" << Total_count
             << ", time=" << endtime - starttime
             << ", throughput="
#if _MSC_VER
             << 1000 * Total_count / (endtime - starttime)
#else
             << (1000000000LL * Total_count) / (endtime - starttime)
#endif
             << endl;
    }
}

// make sure all the parameters make sense
void BenchmarkConfig::verifyParameters()
{
    if ((duration <= 0) && (execute <= 0))
        argError("either 'd' or 'X' must be positive");
    if (datasetsize <= 0)
        argError("m must be positive");
    if (threads <= 0)
        argError("p must be positive");
    if (lookupPct > 96)
        argError("read-only rate is higher than 96%");
    if ((stm_validation != "vis-eager") && (stm_validation != "vis-lazy") &&
        (stm_validation != "invis-eager") && (stm_validation != "invis-lazy") &&
        (stm_validation != "ee") && (stm_validation != "el") &&
        (stm_validation != "ll"))
        argError("Invalid validation strategy");
    if ((stm_validation == "vis-eager" || stm_validation == "vis-lazy") &&
        (threads > 31))
        argError("only up to 31 visible readers are currently supported");
    if (unit_testing != 'l' && unit_testing != 'h' && unit_testing != ' ')
        argError("Invalid unit testing parameter: " + unit_testing);
}

// print parameters if verbosity level permits
void BenchmarkConfig::printConfig()
{
    if (verbosity >= 1) {
        cout << "Bench: use -h for help." << endl;
        cout << bm_name;
        cout << ", d=" << duration << " seconds, m="
             << datasetsize
             << " elements, " << threads << " thread(s)" << endl;
        cout << "Validation Strategy: " << stm_validation << endl;
    }
}

namespace bench
{
  volatile bool early_tx_terminate = false;
}
