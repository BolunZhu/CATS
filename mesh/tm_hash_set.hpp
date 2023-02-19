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

/*  tm_hash_set.hpp
 *
 *  Transactional hash set.
 *  Element type must be the same size as unsigned long.
 *  Currently has only built-in hash function, which is designed to work
 *  well for pointers and stm::sh_ptr<>s.
 */

#ifndef TM_HASH_SET_HPP__
#define TM_HASH_SET_HPP__

#include "tm_set.hpp"
#include "tm_list_set.hpp"

template<typename T>
class tm_hash_set : public tm_set<T>
{
    tm_list_set<T> **bucket;
    int num_buckets;

    tm_hash_set(const tm_hash_set&);
    // no implementation; forbids passing by value
    tm_hash_set& operator=(const tm_hash_set&);
    // no implementation; forbids copying

    // Hash function that should work reasonably well for pointers.
    // Basically amounts to cache line number.
    //
    unsigned long hash(T item)
    {
        // verbose attributing to avoid gcc error
        T*  __attribute__ ((__may_alias__)) f = &item;
        unsigned long*  __attribute__((__may_alias__)) val =
        reinterpret_cast<unsigned long*>(f);
        return (*val >> 6) % num_buckets;
    }

  public:
    virtual void tx_insert(const T item)
    {
        bucket[hash(item)]->tx_insert(item);
    }

    virtual void tx_remove(const T item)
    {
        bucket[hash(item)]->tx_remove(item);
    }

    virtual bool tx_lookup(const T item)
    {
        return bucket[hash(item)]->tx_lookup(item);
    }

    virtual void pv_insert(const T item)
    {
        bucket[hash(item)]->pv_insert(item);
    }

    virtual void pv_remove(const T item)
    {
        bucket[hash(item)]->pv_remove(item);
    }

    virtual bool pv_lookup(const T item)
    {
        return bucket[hash(item)]->pv_lookup(item);
    }

    virtual void pv_apply_to_all(void (*f)(T item))
    {
        for (int i = 0; i < num_buckets; i++)
            bucket[i]->pv_apply_to_all(f);
    }

    tm_hash_set(int capacity)
    {
        // get space for buckets (load factor 1.5)
        num_buckets = capacity + (capacity >> 1);

        bucket = new tm_list_set<T>*[num_buckets];
        for (int i = 0; i < num_buckets; i++)
            bucket[i] = new tm_list_set<T>();
    }

    // Destruction not currently supported.
    virtual ~tm_hash_set() { assert(false); }

    // for debugging (non-thread-safe):
    void print_stats()
    {
        for (int b = 0; b < num_buckets; b++) {
            if (b % 8 == 0)
                cout << "\n";
            cout << "\t" << bucket[b]->pv_size();
        }
        if (num_buckets % 8 != 0)
            cout << "\n";
    }
};

#endif // TM_HASH_SET_HPP__
