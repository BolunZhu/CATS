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
//
///////////////////////////////////////////////////////////////////////////////

#include "rstm.h"
#include <stm/stm.hpp>
#include <stm/support/ThreadLocalPointer.hpp>

//=============================================================================
// Check the build environment.
//=============================================================================
#if defined(STM_LIB_RSTM) || defined(STM_LIB_REDO_LOCK)
#error "The object-based (RSTM and REDO-LOCK) backends are not supported."
#endif

#if !defined(STM_ALLOCATOR_MALLOC)
#error "GCheap unsupported, please reconfigure with malloc."
#endif

#define TO_STRING_LITERAL(arg) #arg
#define MAKE_STR(arg) TO_STRING_LITERAL(arg)



//-----------------------------------------------------------------------------
// CM=[Polite, ...]
//-----------------------------------------------------------------------------
//   The CM define can be used to force a particular contention manager. The
//   flag is ignored if the CM is not supported by the currently configured
//   library. Polite is the default.
//
//   et supports Polite, Aggressive, Timid, Patient, PoliteR, AggressiveR,
//   TimidR, PatientR, where the "R" suffix means to add a bit of randomized
//   exponential backoff on abort (helps with livelock).
//
//   fair supports Patient, PatientR, and C-{K}-{MIN}-{MAX}, where K=number of
//   consecutive aborts after which to increment priority, MIN=min exponential
//   backoff exponent, and MAX=max exponential backoff exponent.
//-----------------------------------------------------------------------------
#if !defined(CM)
#define CM Polite
#endif



//-----------------------------------------------------------------------------
// VERSIONING=[ee, el, ll]
//-----------------------------------------------------------------------------
//   The VERSIONING define can be used to force a particular write acquisition
//   and versioning algorithm at compile time. This is normally a runtime
//   parameter, but we don't want to modify the STAMP applications more than we
//   have to. The default is eager-eager.
//
//   The flag is ignored in runtimes that don't provide the corresponding
//   acquisition/versioning pair.
//-----------------------------------------------------------------------------
#if !defined(VERSIONING)
#define VERSIONING ee
#endif



//-----------------------------------------------------------------------------
// The thread local started flag lets us detect an attempt to start a thread
// more than once.
//-----------------------------------------------------------------------------
static stm::ThreadLocalPointer<bool> started;

// Provide an initialization for the thread local pointer's thr_local key
#if defined(LOCAL_POINTER_ANNOTATION)
template <> LOCAL_POINTER_ANNOTATION
bool* stm::ThreadLocalPointer<bool>::thr_local_key = NULL;
#endif



//-----------------------------------------------------------------------------
// tm_main_startup()
//-----------------------------------------------------------------------------
//   We expect this to be called by the "master" thread in the TM client
//   application. It's ok if this thread is called more than once, or by a
//   non-master thread. It has the same internal functionality as
//   tm_start(...), without returning a descriptor.
//
//   We use thread local storage to make sure that multiple calls from the same
//   thread do not result in multiple stm::init(...)s.
//-----------------------------------------------------------------------------
void tm_main_startup() {
  if (!started.get()) {
    // We use some compiler defines to pick different algorithms here. These
    // are normally dynamically selected in our benchmarks, but STAMP's library
    // based interface doesn't provide us that opportunity.
    stm::init(MAKE_STR(CM), MAKE_STR(VERSIONING), true);

    // Indicate that we have called stm::init(...) on this thread already.
    started.set(new bool(true));
  }
}



//-----------------------------------------------------------------------------
// tm_start(desc, id)
//-----------------------------------------------------------------------------
//   STAMP uses this during its main processing loop, once all of the threads
//   have been created and are sitting at a barrier waiting to start. We expect
//   it to be only called once per thread, but it is ok if it is called more
//   than once. We use thread local storage to make sure that each thread only
//   calls our stm::init(...) routine once.
//
//   The thread that called tm_main_startup(...) /can/, but does not have to,
//   call this routine.
//-----------------------------------------------------------------------------
void tm_start(desc_t** desc, int id) {
  if (!started.get()) {
    // We use some compiler defines to pick different algorithms here. These
    // are normally dynamically set in our benchmarks, but STAMP's library
    // based interface doesn't give us that opportunity.
    stm::init(MAKE_STR(CM), MAKE_STR(VERSIONING), true);

    // Indicate that we've called stm::init(...) already on this thread.
    started.set(new bool(true));
  }

  // The desc parameter is an "out" parameter. Return the address of our
  // descriptor.
  *desc = stm::Descriptor::Self.get();
}



//-----------------------------------------------------------------------------
// tm_stop(id)
//-----------------------------------------------------------------------------
//   We expect this to be called at least once, by each thread, before the
//   application shuts down. 
//-----------------------------------------------------------------------------
void tm_stop(int id)  {
  stm::shutdown(id);
}



//-----------------------------------------------------------------------------
// tm_begin_tx(tx, jmpbuf)
//-----------------------------------------------------------------------------
void tm_begin_tx(desc_t* tx, jmp_buf* jmpbuf) {
  static_cast<stm::Descriptor*>(tx)->beginTransaction(jmpbuf);
}


//-----------------------------------------------------------------------------
// tm_end_tx(tx)
//-----------------------------------------------------------------------------
void tm_end_tx(desc_t* tx) {
  static_cast<stm::Descriptor*>(tx)->commit();
}



//-----------------------------------------------------------------------------
// tm_restart()
//-----------------------------------------------------------------------------
void tm_restart(desc_t* tx) {
  static_cast<stm::Descriptor*>(tx)->restart();
}



//-----------------------------------------------------------------------------
// tm_free(tx, addr)
//-----------------------------------------------------------------------------
//   The interface to our transactional allocator. tm_free(...) may be used
//   from either a transactional or nontransactional context. The only rule is
//   that tm_free(...) should only be passed addresses that were allocated by
//   tm_malloc(...).
//
//   tm_malloc(...) and tm_free(...) should be used for memory allocation and
//   deallocation within a transaction. When used in this context, they provide
//   the buffering required to ensure that changes to the allocator's state are
//   only propagated when the transaction commits.
//
//   Note: we only support using malloc as an allocator in the current release
//         due to incompatibility between the way STAMP does allocation and the
//         requirements of our custom GCHeap allocator.
//-----------------------------------------------------------------------------
void tm_free(desc_t* tx, void* addr) {
  static_cast<stm::Descriptor*>(tx)->txfree(addr);
  addr = NULL;
}



//-----------------------------------------------------------------------------
// tm_malloc(tx, sz)
//-----------------------------------------------------------------------------
//   The interface to our transactional allocator. tm_malloc(...) may be used
//   from either a transactional or nontransactional context. The only rule is
//   memory allocated with tm_malloc(...) must be freed with tm_free(...).
//
//   tm_malloc(...) and tm_free(...) should be used for memory allocation and
//   deallocation within a transaction. When used in this context, they provide
//   the buffering required to ensure that changes to the allocator's state are
//   only propagated when the transaction commits.
//
//   Note: we only support using malloc as an allocator in the current release
//         due to incompatibility between the way STAMP does allocation and the
//         requirements of our custom GCHeap allocator.
//-----------------------------------------------------------------------------
void* tm_malloc(desc_t* tx, int sz) {
  return static_cast<stm::Descriptor*>(tx)->alloc(sz);
}



//-----------------------------------------------------------------------------
// tm_read(tx, addr)
//-----------------------------------------------------------------------------
//   Read memory barrier for 32 bit fixed-point data.
//-----------------------------------------------------------------------------
int tm_read(desc_t* tx, int* addr) {
  return static_cast<stm::Descriptor*>(tx)->stm_read(addr);
}



//-----------------------------------------------------------------------------
// tm_readp(tx, addr)
//-----------------------------------------------------------------------------
//   Read memory barrier for pointers.
//-----------------------------------------------------------------------------
void* tm_readp(desc_t* tx, void** addr) {
  return static_cast<stm::Descriptor*>(tx)->stm_read(addr);
}



//-----------------------------------------------------------------------------
// tm_readf(tx, addr)
//-----------------------------------------------------------------------------
//   Read memory barrier for floating point values.
//-----------------------------------------------------------------------------
float tm_readf(desc_t* tx, float* addr) {
  return static_cast<stm::Descriptor*>(tx)->stm_read(addr);
}



//-----------------------------------------------------------------------------
// tm_write(tx, addr, val)
//-----------------------------------------------------------------------------
//   Write memory barrier for 32 bit fixed-point data.
//-----------------------------------------------------------------------------
void tm_write(desc_t* tx, int* addr, int val) {
  static_cast<stm::Descriptor*>(tx)->stm_write(addr, val);
}



//-----------------------------------------------------------------------------
// tm_writep(tx, addr, val)
//-----------------------------------------------------------------------------
//   Write memory barrier for pointers.
//-----------------------------------------------------------------------------
void tm_writep(desc_t* tx, void** addr, void* val) {
  static_cast<stm::Descriptor*>(tx)->stm_write(addr, val);
}



//-----------------------------------------------------------------------------
// tm_write(tx, addr, val)
//-----------------------------------------------------------------------------
//   Write memory barrier for floating point values.
//-----------------------------------------------------------------------------
void tm_writef(desc_t* tx, float* addr, float val) {
  static_cast<stm::Descriptor*>(tx)->stm_write(addr, val);
}
