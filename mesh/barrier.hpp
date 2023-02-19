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

/*  barrier.hpp
 *
 *  Simple barrier.  Currently based on pthread locks.
 *  Note that pthread barriers are an optional part of the standard, and
 *  are not supported on some platforms (including my mac).
 *  If verbose, prints time when barrier is achieved.
 */

#ifndef BARRIER_HPP__
#define BARRIER_HPP__

#include <pthread.h>    // for pthread_cond
#include <string>
    using std::string;

#include "macros.hpp"

class barrier {
    int participants;
    int parity;         // for sense reversal
    pthread_mutex_t mutex;
    int count[2];
    pthread_cond_t sem[2];
public:
    void wait(string s) {
        VERIFY(pthread_mutex_lock(&mutex));
        int my_parity = parity;
        ++count[my_parity];
        if (count[my_parity] == participants) {
            // I was the last to arrive
            count[my_parity] = 0;
            parity = 1 - my_parity;

            if (verbose && s.size() != 0) {
                unsigned long long now = getElapsedTime();
                {
                    with_lock cs(io_lock);
                    cout << "time: " << (now - start_time)/1e9 << " "
                                     << (now - last_time)/1e9
                                     << " (" << s << ")\n";
                }
                last_time = now;
            }
            VERIFY(pthread_cond_broadcast(&sem[my_parity]));
        } else {
            while (!count[my_parity] == 0) {
                VERIFY(pthread_cond_wait(&sem[my_parity], &mutex));
            }
        }
        VERIFY(pthread_mutex_unlock(&mutex));
    }
    barrier(int n) : participants(n) {
        count[0] = count[1] = 0;
        parity = 0;
        VERIFY(pthread_mutex_init(&mutex, 0));
        VERIFY(pthread_cond_init(&sem[0], 0));
        VERIFY(pthread_cond_init(&sem[1], 0));
    }
    ~barrier() {
        VERIFY(pthread_mutex_destroy(&mutex));
        VERIFY(pthread_cond_destroy(&sem[0]));
        VERIFY(pthread_cond_destroy(&sem[1]));
    }
};

#endif // BARRIER_HPP__
