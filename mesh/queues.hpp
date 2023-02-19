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

/*  queues.hpp
 *
 *  Sequential and concurrent queues.
 *  Element type should always be (the same size as) a pointer.
 */

#ifndef QUEUES_HPP__
#define QUEUES_HPP__

#include <cassert>
#include <queue>
    using std::queue;

#include "config.hpp"

template<typename T>
class simple_queue {
  public:
    virtual void enqueue(const T item, const int tid) = 0;
    virtual T dequeue(const int tid) = 0;
    virtual ~simple_queue() { }
    // so compiler won't complain
};

template<typename T>
class sequential_queue : public simple_queue<T>, private queue<T> {
  public:
    virtual void enqueue(const T item, const int ignored) {
        assert(item != 0);
        push(item);
    }
    virtual T dequeue(int ignored) {
        if (this->empty()) return T(0);
        T rtn = this->front();
        this->pop();
        return rtn;
    }
    virtual ~sequential_queue() { }
};

//  Counted pointers incorporate a serial number to avoid the A-B-A problem
//  in shared data structures.  They are needed for algorithms based on
//  compare-and-swap.  We could get by without them if we had LL/SC.
//
union counted_ptr
{
    struct {
#if defined(__i386__) || defined(_MSC_VER) /* little endian */
        volatile unsigned long sn;      // serial number
        void* volatile ptr;
#elif defined(__sparc__) /* big endian */
        void* volatile ptr;
        volatile unsigned long sn;      // serial number
#else
#error Please indicate your endianness here.
#endif
    } p;
    volatile unsigned long long all;    // For when we need to read the whole
                                        // thing at once
} __attribute__ ((aligned(8)));

//  Class exists only as a (non-templated) base for concurrent_queue<T>
//
class MS_queue {
    counted_ptr head;
    counted_ptr tail;
  protected:
    void enqueue(void* item, const int tid);
    void* dequeue(const int tid);
    MS_queue(const int tid);
    ~MS_queue() { assert(false); }
    // Destruction of concurrent queue not currently supported.
};

template<typename T>
class concurrent_queue : public simple_queue<T>, private MS_queue
{
    concurrent_queue(const concurrent_queue&);
    // no implementation; forbids passing by value
    concurrent_queue& operator=(const concurrent_queue&);
    // no implementation; forbids copying

    // note that assert in constructor guarantees that items and
    // void*s are the same size
    //
    static void* anonymize(T item) {
        union { void** i; T* t; } u;
        u.t = &item;
        return *u.i;
    }
    static T unanonymize(void* p) {
        union { void** i; T* t; } u;
        u.i = &p;
        return *u.t;
    }

  public:
    virtual void enqueue(T item, const int tid) {
        MS_queue::enqueue(anonymize(item), tid);
    }
    virtual T dequeue(const int tid) {
        return unanonymize(MS_queue::dequeue(tid));
    }

    // make sure that the queue is cache-line aligned
    static void* operator new(const size_t size) {
        return memalign(CACHELINESIZE, size);
    }
    // the C++ runtime's delete operator should do this the same way,
    // but just in case...
    static void operator delete(void *ptr) {free(ptr);}

    concurrent_queue(const int tid) : MS_queue(tid) {
        assert(sizeof(T) == sizeof(void*));
    }
    virtual ~concurrent_queue() { }
};

#endif // QUEUES_HPP__
