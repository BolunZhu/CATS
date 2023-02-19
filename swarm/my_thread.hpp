/******************************************************************************
 *
 * Copyright (c) 2007, 2008, 2009
 * University of Rochester
 * Department of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the name of the University of Rochester nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*  my_thread.hpp     (the name "thread.h" is taken)
 *
 *  Simple wrapper for pthreads.
 *  Currently uses default attributes, except for PTHREAD_SCOPE_SYSTEM,
 *  which allows threads to be scheduled on multiple processors.
 */

#ifndef __MY_THREAD_H__
#define __MY_THREAD_H__

#include <cstdlib>     // for exit()

#ifndef _MSC_VER
#include <pthread.h>
#include <sys/signal.h>
#endif

#include "Game.hpp"
#include <iostream>
using std::cerr;

#define VERIFY(E)                                                   \
    {int stat = (E);                                                \
        if (stat != 0) {                                            \
            cerr << __FILE__ << "[" << __LINE__ << "] bad return("  \
                 << stat << "): " << strerror(stat) << "\n";        \
            exit(-1);                                               \
        }}


/* abstract */
class runnable {
  public:
    virtual void operator()() = 0;
    virtual ~runnable() { }
};

#ifdef _MSC_VER
extern DWORD WINAPI call_runnable(LPVOID f);
#else
extern void *call_runnable(void *f);
#endif

class thread {

#ifndef _MSC_VER
    pthread_t self;
#else
    HANDLE self;
#endif

  public:
    // start immediately upon creation:
    thread(runnable *f) {
#ifndef _MSC_VER
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        VERIFY(pthread_create(&self, &attr, call_runnable, f));
#else

        self = CreateThread(NULL, 0, call_runnable, f, 0, NULL);
#endif
    }

    // join by deleting:
    ~thread() {
#ifndef _MSC_VER
        VERIFY(pthread_join(self, 0));
#else
        if (self)
        {
            WaitForSingleObject(self, INFINITE);
            CloseHandle(self);
            self = NULL;
        }
#endif
    }
};

#endif // __MY_THREAD_H__
