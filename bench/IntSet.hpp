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

#ifndef INTSET_HPP__
#define INTSET_HPP__

#include <iostream>
#include "Benchmark.hpp"

using std::cerr;
using std::endl;

// common interface for all benchmarks that support the insertion, removal, and
// lookup of a single integer value in a data structure
class IntSet
{
  public:
    virtual bool lookup(int val) const = 0;
    virtual void insert(int val) = 0;
    virtual void remove(int val) = 0;

    // perform a sanity check to ensure that the data structure integrity
    // hasn't been compromised
    virtual bool isSane(void) const = 0;
    virtual ~IntSet() { }
};

// since we've got two trees, let's put the code here
enum Color { RED, BLACK };

class IntSetBench : public Benchmark
{
    IntSet* S;
    int M;
  public:
    IntSetBench(IntSet* s, int m) : S(s), M(m) { }
    void random_transaction(thread_args_t* args, unsigned int* seed,
                            unsigned int val,    int action);
    bool sanity_check() const;
    // all of the intsets have the same verification code
    virtual bool verify(VerifyLevel_t v);
};

inline
void IntSetBench::random_transaction(thread_args_t* args, unsigned int* seed,
                                     unsigned int val,    int action)
{
    val %= M;

    if (action < BMCONFIG.lookupPct) {
        if (S->lookup(val)) {
            ++args->count[TXN_LOOKUP_TRUE];
        }
        else {
            ++args->count[TXN_LOOKUP_FALSE];
        }
    }
    else if (action < BMCONFIG.insertPct) {
        S->insert(val);
        ++args->count[TXN_INSERT];
    }
    else {
        S->remove(val);
        ++args->count[TXN_REMOVE];
    }
}

inline bool IntSetBench::sanity_check() const
{
    return S->isSane();
}

// verify() is a method for making sure that the data structure works as
// expected
inline bool IntSetBench::verify(VerifyLevel_t v)
{
    // always do the light verification:
    int N = 256;
    for (int k = 1; k <= N; k++) {
        for (int i = k; i <= N; i += k) {
            if (S->lookup(i))
                S->remove(i);
            else
                S->insert(i);
            if (!S->isSane()) return false;
        }
    }
    int j = 1;
    for (int k = 1; k <= N; k++) {
        if (k == j*j) {
            if (!S->lookup(k)) return false;
            j++;
        } else {
            if (S->lookup(k)) return false;
        }
    }
    // clean out the data structure
    for (int k = 1; k <= N; k++) {
        S->remove(k);
    }

    // maybe do the heavy verification (1M random ops):
    if (v == HEAVY) {
        bool val[256];
        for (int i = 0; i < 256; i++) {
            val[i] = false;
        }
        int op_count = 0;
        for (int i = 0; i < 1000000; i++) {
            if (++op_count % 10000 == 0) {
                cerr << ".";
                // op_count = 0;
            }
            int j = rand() % 256;
            int k = rand() % 3;
            if (k == 0) {
                val[j] = false;
                S->remove(j);
            }
            else if (k == 1) {
                val[j] = false;
                S->remove(j);
            }
            else {
                if (S->lookup(j) != val[j]) {
                    std::cerr << "Value error!" << std::endl;
                    return false;
                }
            }
            if (!S->isSane()) {
                std::cerr << "Sanity check error!" << std::endl;
                return false;
            }
        }
    }
    cerr << endl;
    return true;
}

#endif // INTSET_HPP__
