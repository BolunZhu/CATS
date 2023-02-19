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

#ifndef __THREADLOCAL_H__
#define __THREADLOCAL_H__

/**
 *  To use a ThreadLocalPointer, declare one in a header like this:
 *    extern ThreadLocalPointer<TxHeap> currentHeap;
 *
 *  Then you must back the pointer in a cpp file like this:
 *    ThreadLocalPointer<TxHeap> currentHeap;
 *    #if defined(LOCAL_POINTER_ANNOTATION)
 *    template <> LOCAL_POINTER_ANNOTATION TxHeap*
 *    ThreadLocalPointer<TxHeap>::thr_local_key = NULL;
 *    #endif
 */

#include "defs.hpp"

/**
 *  This #if block serves three roles:
 *    - It includes files as necessary for different pthread implementations
 *    - It hides differences between VisualC++ and GCC syntax
 *    - It makes sure that a valid thread-local option is specified so that
 *      subsequent #ifs don't have to do error checking
 */
#if defined(STM_TLS_PTHREAD)
#include <pthread.h>
#elif defined(STM_TLS_GCC)
#define LOCAL_POINTER_ANNOTATION      __thread
#elif defined(_MSC_VER)
#include <windows.h>
#define LOCAL_POINTER_ANNOTATION      __declspec(thread)
#else
#error "Invalid or Missing ThreadLocal #define"
#endif

namespace stm
{
  /**
   *  Hide the fact that some of our platforms use pthread_getspecific while
   *  others use os-specific thread local storage mechanisms.  In all cases, if
   *  you instantiate one of these with a T, then you'll get a thread-local
   *  pointer to a T that is easy to access and platform independent
   */
  template<class T>
  class ThreadLocalPointer
  {
      /**
       * either declare a key for interfacing to PTHREAD thread local
       * storage, or else declare a static thread-local var of type T*
       */
#if defined(STM_TLS_PTHREAD)
      pthread_key_t thr_local_key;
#else
      static LOCAL_POINTER_ANNOTATION T* thr_local_key;
#endif

    public:
      /**
       *  Perform global initialization of the thread key, if applicable.  Note
       *  that we have a default value of NULL.  You can't store a NULL value
       *  in this thread-local pointer if you want to do subsequent tests to
       *  ensure that the variable is initialized for your thread.
       */
      ThreadLocalPointer()
      {
#if defined(STM_TLS_PTHREAD)
          pthread_key_create(&thr_local_key, NULL);
#else
          thr_local_key = NULL;
#endif
      }

      /*** Free the thread local key or set the pointer to NULL */
      ~ThreadLocalPointer()
      {
#if defined(STM_TLS_PTHREAD)
          pthread_key_delete(thr_local_key);
#else
          thr_local_key = NULL;
#endif
      }

      /*** Get the pointer stored for this thread */
      T* const get() const
      {
#if defined(STM_TLS_PTHREAD)
          return static_cast<T* const>(pthread_getspecific(thr_local_key));
#else
          return thr_local_key;
#endif
      }

      /***  Set this thread's pointer */
      void set(T* val)
      {
#if defined(STM_TLS_PTHREAD)
          pthread_setspecific(thr_local_key, (void*)val);
#else
          thr_local_key = val;
#endif
      }

      /*** operators for dereferencing */
      T* const operator->() const { return get(); }
      T& operator*() const { return *get(); }
  };
} // stm

#endif // __THREADLOCAL_H__
