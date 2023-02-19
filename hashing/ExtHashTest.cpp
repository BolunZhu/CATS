///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2008, 2009
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

// all of our released stms use an assertion to test that a transaction doesn't
// try to become inevitable after performing reads/writes.  For this workload,
// we need to become inevitable after performing reads.  It should be safe to
// just skip the assertion in try_inevitable, so we'll hack the NDEBUG define
// to ensure that assertions are turned off
#ifndef NDEBUG
#define NDEBUG
#define TURNED_OFF_NDEBUG
#endif

#include <pthread.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <vector>

// we need to be sure unptr use in transactions is safe, even if it was
// incorrectly turned on in config.h
#define STM_IGNORE_UNPTR_TESTS
#include <stm/stm.hpp>

#include "ExtensibleHash.hpp"

using std::string;
using std::cout;
using std::endl;
using std::vector;

/*****************************************************************************
 *
 * Simple benchmark: threads perform random operations on hash tables.  When a
 * hash table's chains start getting too big, we rehash it.
 *
 *****************************************************************************/

/***  Struct to hold all configuration information */
struct config_t
{
    int    threads; // total thread count
    int    iterations; // total transactions by workers
    int    num_collections; // number of hash tables
    int    collections_per_tx; // how many hashtable ops per transaction
    int    hash_initial_buckets; // initial size of the hash tables
    int    hash_chain_depth; // when more than this many elements are in a
                             // hash, rehash
    bool   rehash_thread; // should one of the threads be dedicated for
                          // rehashing?
    bool   inev_rehash; // should rehashing be inevitable?
    int    prio_rehash; // how much priority should the rehasher get?
    bool   priv_rehash; // should rehashing be via privatization
    string validation; // validation choice for the runtime
    string cm; // cm choice for the runtime

    /***  default is to run a 1000 iteration test with 2 threads */
    config_t()
        : threads(2), iterations(1000), num_collections(4),
          collections_per_tx(8), hash_initial_buckets(8), hash_chain_depth(4),
          rehash_thread(false), inev_rehash(false), prio_rehash(0),
          priv_rehash(false),
          validation("invis-eager"), cm("Polka")
    { }

    /***  Display the config parameters */
    void display()
    {
        cout << "Sensors: p=" << threads << ", i=" << iterations
             << ", c=" << num_collections <<", x=" << collections_per_tx
             << ", b=" << hash_initial_buckets <<", d=" << hash_chain_depth
             << ", r=" << rehash_thread <<", I=" << inev_rehash
             << ", P=" << prio_rehash <<", Z=" << priv_rehash
             << ", V=" << validation << ", C=" << cm << endl;
    }
};

/*** Global config options */
config_t CONFIG;

class Entry : public stm::Object
{
    /*** when was entry created */
    GENERATE_FIELD(int, timestamp);

    /*** entries contain a single ulong */
    GENERATE_FIELD(unsigned long, contents);

    /*** thread who made this entry */
    GENERATE_FIELD(int, parent);

  public:
    Entry(int t, unsigned long c, int r)
        : m_timestamp(t), m_contents(c), m_parent(r)
    { }

    Entry() : m_timestamp(0), m_contents(0), m_parent(0) { }
};

/*** convenience typedef */
typedef ExtensibleHash<unsigned long, stm::sh_ptr<Entry> > EntryHash;

/*** set of collections */
EntryHash** HASH;

/*** global timing information */
unsigned long long STARTTIME = 0;
unsigned long long ENDTIME   = 0;

/*** barrier to synchronize timing */
void barrier(int id, unsigned long nthreads)
{
    static struct {
        volatile unsigned long count;
        volatile unsigned long sense;
        volatile unsigned long thread_sense[256]; // hard-coded max threads
    } __attribute__ ((aligned(64))) bar = {0};

    bar.thread_sense[id] = !bar.thread_sense[id];
    if (fai(&bar.count) == nthreads - 1) {
        bar.count = 0;
        bar.sense = !bar.sense;
    }
    else {
        while (bar.sense != bar.thread_sense[id]) { }     // spin
    }
}

/*** lightweight barrier for synchronizing the rehasher with producers */
volatile unsigned long experiment_over = 0;

/**
 *  This is the packet of information that describes what a thread ought to do
 *  and what it has done
 */
struct thread_args_t
{
    /*** my thread id */
    int id;

    /** total number of threads (needed for barriers) */
    int threads;

    /*** random seed */
    unsigned int seed;

    /*** count of transactions that have completed */
    unsigned long long txns;
};

// thread that populates hashes
void producer_thread(thread_args_t* args)
{
    // divide the work across the total number of producer threads
    unsigned workers = CONFIG.threads;
    if (CONFIG.rehash_thread && workers > 1)
        workers--;
    // do this thread's transactions:
    for (unsigned q = 0; q < (CONFIG.iterations / workers); q++) {
        // pick the hash tables to access in this transaction
        unsigned hashes[CONFIG.collections_per_tx];
        for (int i = 0; i < CONFIG.collections_per_tx; i++) {
            hashes[i] = rand_r(&args->seed) % CONFIG.num_collections;
        }

        // get values for the hashes we'll access
        unsigned long val[CONFIG.collections_per_tx];
        for (int i = 0; i < CONFIG.collections_per_tx; i++) {
            val[i] = rand_r(&args->seed);
        }

        // insert values into hashes
        BEGIN_TRANSACTION {
            for (int i = 0; i < CONFIG.collections_per_tx; i++) {
                // use rand_r to create a key for this packet from the val
                unsigned int tseed = val[i];
                unsigned long key = rand_r(&tseed);
                stm::sh_ptr<Entry> ent(new Entry(i, val[i], args->id));
                HASH[hashes[i]]->insert_tx(key, ent);
            }
        } END_TRANSACTION;

        args->txns++;
    }
}

// thread that does rehashing
void rehash_thread(thread_args_t* args)
{
    int startpoint = 0;
    while (experiment_over == 0) {
        int rehashed = -1;
        int priv_hash = -1;
        // check for a table to rehash
        BEGIN_TRANSACTION {
            rehashed = -1;
            priv_hash = -1;
            // go through all hashes, but start from where we left off
            for (int i = 0; i < CONFIG.num_collections; i++) {
                int index = (startpoint + i) % CONFIG.num_collections;
                // if this table needs to be rehashed, do it as appropriate for
                // this configuration
                if (HASH[index]->needRehash()) {
                    if (CONFIG.inev_rehash) {
                        stm::try_inevitable();
                        HASH[index]->reHash_tx();
                        rehashed = index;
                        break;
                    }
                    else if (CONFIG.prio_rehash) {
                        stm::setPrio(CONFIG.prio_rehash);
                        HASH[index]->reHash_tx();
                        rehashed = index;
                        break;
                    }
                    else if (CONFIG.priv_rehash) {
                        priv_hash = index;
                        rehashed = index;
                        break;
                    }
                    else {
                        HASH[index]->reHash_tx();
                        rehashed = index;
                        break;
                    }
                }
            }
            // if I didn't rehash anything, wait until someone sets a flag
            if (rehashed == -1 && experiment_over == 0 && priv_hash == -1)
                stm::retry();
        } END_TRANSACTION;
        if (priv_hash != -1) {
            HASH[priv_hash]->reHash_privately();
        }

        startpoint = (rehashed+1)%CONFIG.num_collections;
    }
    // ack that I'm done
    experiment_over = 2;
}

/*** new threads will all begin in this code */
void* hash_test(void* arg)
{
    thread_args_t* args = (thread_args_t*)arg;

    // threads other than thread0 must create a transactional context here
    if (args->id != 0)
        stm::init(CONFIG.cm, CONFIG.validation, true);

    // all thread configuration before we start timing
    barrier(args->id, args->threads);

    // get the start time of the benchmark run
    if (args->id == 0)
        STARTTIME = getElapsedTime();

    // start a rehash thread only if the -f flag is > 0
    if (CONFIG.rehash_thread > 0) {
        if (args->id == 1)
            rehash_thread(args);
        else
            producer_thread(args);

        // wait for all network listeners to exit
        if (args->id != 1)
            barrier(args->id, args->threads - 1);

        // now shut down the rehasher
        if (args->id == 0) {
            stm::halt_retry();
            experiment_over = 1;
        }

        if (args->id == 0) {
            while (experiment_over != 2) { }
        }
    }
    // -f is 0, so everyone is a network_listener
    else {
        producer_thread(args);

        // wait for all threads to exit
        barrier(args->id, args->threads);
    }

    // everyone is done, so we can get the endtime here
    if (args->id == 0)
        ENDTIME = getElapsedTime();

    // Shut down STM for all threads other than thread 0
    if (args->id != 0)
        stm::shutdown(args->id);

    return 0;
}

/*** Print usage */
void usage()
{
    cout << "Valid options are:" << endl;
    cout << "  -p : threads (default 2)" << endl;
    cout << "  -i : iterations (default 1000)" << endl;
    cout << "  -c : number of collections to maintain (default 4)" << endl;
    cout << "  -x : number of collections to access per transaction (default 8)" << endl;
    cout << "  -b : number of buckets in new hashes" << endl;
    cout << "  -d : maximum depth of a hash bucket before rehashing" << endl;
    cout << "  -r : run a separate rehash thread" << endl;
    cout << "  -I : use inevitable rehashing" << endl;
    cout << "  -P : use priority rehashing" << endl;
    cout << "  -V : validation strategy" << endl;
    cout << "  -C : contention manager" << endl;
    cout << "  -h : this message" << endl;
}

/***  Main driver for the benchmark */
int main(int argc, char** argv)
{
    // parse the command-line options
    int opt;
    while ((opt = getopt(argc, argv, "p:i:c:x:b:d:rIP:ZC:V:h")) != -1) {
        switch (opt) {
          case 'p':
            CONFIG.threads = atoi(optarg);
            assert(CONFIG.threads > 0);
            break;
          case 'i':
            CONFIG.iterations = atoi(optarg);
            assert(CONFIG.iterations > 0);
            break;
          case 'c':
            CONFIG.num_collections = atoi(optarg);
            assert(CONFIG.num_collections > 0);
            break;
          case 'x':
            CONFIG.collections_per_tx = atoi(optarg);
            assert(CONFIG.collections_per_tx > 0);
            break;
          case 'b':
            CONFIG.hash_initial_buckets = atoi(optarg);
            assert(CONFIG.hash_initial_buckets > 0);
            break;
          case 'd':
            CONFIG.hash_chain_depth = atoi(optarg);
            assert(CONFIG.hash_chain_depth > 0);
            break;
          case 'r':
            CONFIG.rehash_thread = true;
            break;
          case 'I':
            CONFIG.inev_rehash = true;
            break;
          case 'P':
            CONFIG.prio_rehash = atoi(optarg);
            assert(CONFIG.prio_rehash > 0);
            break;
          case 'Z':
            CONFIG.priv_rehash = true;
            break;
          case 'V':
            CONFIG.validation = string(optarg);
            break;
          case 'C':
            CONFIG.cm = string(optarg);
            break;
          case 'h':
            usage();
            return 0;
          default:
            usage();
            return -1;
        }
    }

    // print this experiment's information
    CONFIG.display();

    // get a transactional context
    stm::init(CONFIG.cm, CONFIG.validation, true);

    // construct the hashes.  malloc() is fine for this
    HASH = new EntryHash* [CONFIG.num_collections];
    for (int i = 0; i < CONFIG.num_collections; i++) {
        HASH[i] = new EntryHash(CONFIG.hash_initial_buckets,
                                 CONFIG.hash_chain_depth);
    }

    // create enough config packets for all threads
    vector<thread_args_t> args(CONFIG.threads);

    // create enough pthread handles for all threads
    vector<pthread_t> tid(CONFIG.threads);

    // create pthread scope to support multithreading
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    // set up configuration structs for the threads we'll create
    for (int i = 0; i < CONFIG.threads; i++) {
        // thread id, number of threads
        args[i].id = i;
        args[i].seed = i;
        args[i].threads = CONFIG.threads;

        // set counts to zero
        args[i].txns = 0;
    }

    // actually create the threads
    for (int j = 1; j < CONFIG.threads; j++)
        pthread_create(&tid[j], &attr, &hash_test, &args[j]);

    // all of the other threads should be queued up, waiting to run the
    // benchmark, but they can't until this thread starts the benchmark too...
    hash_test((void*)(&args[0]));

    // everyone should be done.  Join all threads so we don't leave anything
    // hanging around
    for (int k = 1; k < CONFIG.threads; k++)
        pthread_join(tid[k], NULL);

    // output total performance
    cout << "total transactions = " << CONFIG.iterations << endl;
    cout << "total time = " << ENDTIME - STARTTIME << endl;
    cout << (1000000000LL * CONFIG.iterations)/(ENDTIME-STARTTIME)
         << " transactions per second" << endl;

    // shut off transactions in thread 0
    stm::shutdown(0);
}
