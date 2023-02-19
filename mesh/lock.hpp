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

/*  lock.hpp
 *
 *  Synchronization currently uses tatas locks but pthread barriers.
 *  Should use something faster.
 */

#ifndef THREAD_HPP__
#define THREAD_HPP__

#include <sys/errno.h>                  // for EDEADLK
#include <stm/support/atomic_ops.h>        // for tatas_lock_t

class d_lock {
    tatas_lock_t mutex;
    int count;
public:
    void acquire() {
        tatas_acquire(&mutex);
        ++count;
    }
    void release() {
        if (--count == 0)
            tatas_release(&mutex);
    }
    d_lock() {
        mutex = 0;
        count = 0;
    }
};

// Declaring one of these makes a scope a critical section.
//
class with_lock {
    d_lock *my_lock;
public:
    with_lock(d_lock &l) {
        my_lock = &l;
        my_lock->acquire();
    }
    ~with_lock() {
        my_lock->release();
    }
};

#endif // MESH_HPP__
