///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007, 2008, 2009
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

#ifndef BLOOM_HPP__
#define BLOOM_HPP__

#include <stdlib.h>
#include <cassert>

#ifdef __SSE__
#include <xmmintrin.h>
#include <mmintrin.h>
#endif

#ifdef _MSC_VER
#include "../../alt-license/rand_r.h"
#endif

/**
 *  Templated Bloom Filter.  The template parameters are size (in bits) and
 *  number of hash functions.  Valid HASHES values are 1 and 3.
 */
template <int SIZE, int HASHES>
class Bloom
{
    void insert1(unsigned val) volatile
    {
        // no need to be fancy... just use the address
        unsigned key = (val >> 3) % SIZE;

        // set the bits that correspond to the keys
        unsigned index = key / (8*sizeof(unsigned long));
        unsigned bit = key % (8*sizeof(unsigned long));
        unsigned long mask = 1L << bit;
        filter[index] |= mask;
    }

    bool lookup1(unsigned val) volatile
    {
        // again, no need to be fancy, just use the address
        unsigned key = (val >> 3) % SIZE;

        unsigned index = key / (8*sizeof(unsigned long));
        unsigned bit = key % (8*sizeof(unsigned long));
        unsigned long mask = 1L << bit;
		// Suppress a visual studio performance warning
#if defined(_MSC_VER)
		return (filter[index] & mask) ? true : false;
#else
		return (filter[index] & mask);
#endif
	}

    void insert2(unsigned val) volatile
    {
        // use contiguous bit ranges
        unsigned key[2];
        key[0] = (val >> 3) % SIZE;
        key[1] = ((val >> 3) / SIZE) % SIZE;

        // set the bits that correspond to the keys
        for (int k = 0; k < 2; k++) {
            unsigned index = key[k] / (8*sizeof(unsigned long));
            unsigned bit = key[k] % (8*sizeof(unsigned long));
            unsigned long mask = 1 << bit;
            filter[index] |= mask;
        }
    }

    bool lookup2(unsigned val) volatile
    {
        // use contiguous bit ranges
        unsigned key[2];
        key[0] = (val >> 3) % SIZE;
        key[1] = ((val >> 3) / SIZE) % SIZE;

        // test the bits that correspond to the keys; if any fail, return 0
        int k;
        for (k = 0; k < 2; k++) {
            unsigned index = key[k] / (8*sizeof(unsigned long));
            unsigned bit = key[k] % (8*sizeof(unsigned long));
            unsigned long mask = 1 << bit;
            if (!(filter[index] & mask))
                return false;
        }
        return true;
    }

    void insert3(unsigned val) volatile
    {
        // we're going to use 3 keys: Key 1 will be the value itself.  We'll
        // use the value as a seed to rand_r to get key 2, and we'll use the
        // new seed (from rand_r) as key 3.
        unsigned key[3];
        key[0] = (val >> 3) % SIZE;
        key[2] = val;
        key[1] = rand_r(&key[2]) % SIZE;
        key[2] %= SIZE;

        // set the bits that correspond to the keys
        for (int k = 0; k < 3; k++) {
            unsigned index = key[k] / (8*sizeof(unsigned long));
            unsigned bit = key[k] % (8*sizeof(unsigned long));
            unsigned long mask = 1 << bit;
            filter[index] |= mask;
        }
    }

    bool lookup3(unsigned val) volatile
    {
        // we're going to use 3 keys: Key 1 will be the value itself.  We'll
        // use the value as a seed to rand_r to get key 2, and we'll use the
        // new seed (from rand_r) as key 3.
        unsigned key[3];
        key[0] = (val >> 3) % SIZE;
        key[2] = val;
        key[1] = rand_r(&key[2]) % SIZE;
        key[2] %= SIZE;

        // test the bits that correspond to the keys; if any fail, return 0
        int k;
        for (k = 0; k < 3; k++) {
            unsigned index = key[k] / (8*sizeof(unsigned long));
            unsigned bit = key[k] % (8*sizeof(unsigned long));
            unsigned long mask = 1 << bit;
            if (!(filter[index] & mask))
                return false;
        }
        return true;
    }

  public:
    // __attribute__((aligned(16))) isn't working, so we need to do things
    // oddly here to get sse to work
    //
    // NB: extra two words because __attribute__((aligned(8))) works
    volatile unsigned long
    __attribute__((aligned(8)))
    true_filter[(SIZE)/(8*sizeof(unsigned long)) + 2];

    volatile unsigned long* filter;
#ifdef __SSE__
    __m128i*       sse_filter;
#endif

    /**
     *  Construct a bloom filter:
     *    Ensure that the number of buckets is a multiple of 8, and then clear
     *    the filter
     */
    Bloom()
    {
        assert((HASHES == 1) || (HASHES == 2) || (HASHES == 3));
        assert(SIZE % (8*sizeof(unsigned long)) == 0);

        if (((unsigned)&true_filter)%16 == 8)
            filter = &true_filter[2];
        else
            filter = &true_filter[0];
#ifdef __SSE__
        sse_filter = (__m128i*)filter;
#endif
        reset();
    }

    /**
     *  clear a filter...
     */
    void reset() volatile
    {
#ifdef __SSE__
        for (unsigned i = 0; i < SIZE/(8*sizeof(__m128i)); i++)
            sse_filter[i] = _mm_setzero_si128();
#else
        for (unsigned i = 0; i < SIZE/(8*sizeof(unsigned long)); i++)
            filter[i] = 0;
#endif
    }

    /**
     *  Insert a new entry
     *  The compiler should eliminate the branches in this code
     */
    void insert(unsigned val) volatile
    {
        if (HASHES == 1)
            insert1(val);
        else if (HASHES == 2)
            insert2(val);
        else if (HASHES == 3)
            insert3(val);
    }

    /**
     *  Lookup an entry
     *  The compiler should eliminate the branches in this code
     */
    bool lookup(unsigned val) volatile
    {
        if (HASHES == 1)
            return lookup1(val);
        else if (HASHES == 2)
            return lookup2(val);
        else if (HASHES == 3)
            return lookup3(val);
        else
            return false;
    }

    /**
     * intersect two filters...
     */
    bool intersect(volatile Bloom<SIZE, HASHES>* other) volatile
    {
#ifdef __SSE__
        union {
            __m128i vec;
            unsigned words[4];
        } res;
        res.vec = _mm_setzero_si128();

        for (unsigned i = 0; i < SIZE/(8*sizeof(__m128i)); i++)
            res.vec = _mm_or_si128(res.vec,
                                   _mm_and_si128(sse_filter[i],
                                                 other->sse_filter[i]));
        return !((res.words[0] == 0) && (res.words[1] == 0) &&
                (res.words[2] == 0) && (res.words[3] == 0));
#else
        for (unsigned i = 0; i < SIZE/(8*sizeof(unsigned long)); i++)
            if (filter[i] & other->filter[i])
                return true;
        return false;
#endif
    }
};

#endif // BLOOM_HPP__
