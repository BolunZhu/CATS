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

#ifndef __DEFS_HPP__
#define __DEFS_HPP__

#include <config.h>
#include <stddef.h>
#include <cstdlib>

// Macros to let certain GCC-isms compile MSVC and MSVC-isms compile with GCC.
#ifdef _MSC_VER
#define __attribute__(x)
#else
#define __declspec(x)
#endif

// for exiting (somewhat gracefully) upon an unrecoverable error
#define UNRECOVERABLE_ERROR(x) { std::cerr << "Unrecoverable Error: " << x << std::endl << std::flush; exit(-1); }

namespace stm
{
  /**
   *  Retry() and Aborted() are deprecated.  Any rollback should simply use
   *  RollBack().  In reality, RollBack is deprecated too.  setjmp should be
   *  used.
   */
  class RollBack { };

  /**
   *  Transactions must be in one of these states
   *  NB:  FINISHING is not in use
   */
  enum TxState { ACTIVE = 0, ABORTED = 1, FINISHING = 2, COMMITTED = 3 };

#define MAX_THREADS 256

  /**
   *  Forward declarations for implementation-specific logged MM calls and
   *  deferred destruction.  These live in namespace STM, and the api file
   *  forwards them to the appropriate descriptor.
   */
  void* tx_alloc(size_t size);
  void tx_free(void* ptr);

  /**
   *  If ALLOCATION_GCHEAP isn't set, then we need to provide the same public
   *  functions as GCHeap... otherwise, just forward declare those functions
   *  that are visible from GCHeap.cpp
   */
  namespace mm
  {
#if !defined(STM_ALLOCATOR_GCHEAP_NOEPOCH) && !defined(STM_ALLOCATOR_GCHEAP_EPOCH)
    inline void txfree(void* mem)     { free(mem); }
    inline void* txalloc(size_t size) { return malloc(size);}
    inline void initialize()          { }
    inline void assert_not_deleted(void* ptr) { }
#else
    // this is "free" for whatever allocator you are using
    void txfree(void* mem);
    // this is "malloc" for whatever allocator you are using
    void* txalloc(size_t size);
    void initialize();
    void assert_not_deleted(void* ptr);
#endif

    /**
     *  Master class for all objects that are used in transactions, to ensure
     *  that those objects have logged allocation/free and deferred destruction
     */
    class CustomAllocedBase
    {
      public:
        void* operator new(size_t size) { return tx_alloc(size); }
        void operator delete(void* ptr) { txfree(ptr); }
        virtual ~CustomAllocedBase() { }
    };
  }
}

#if defined(STM_SIMULATOR_MINIMAL) || defined(STM_SIMULATOR_FULL)
#include "simulator.hpp"
#endif

/**
 *  Always provide the STM Allocator for STL objects who need to use
 *  txfree/txalloc for deferred reclamation
 */
#include "StlAllocator.hpp"

#endif // __DEFS_HPP__
