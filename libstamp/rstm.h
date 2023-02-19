//////////////////////////////////////////////////////////////////-*- C++ -*-//
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

//=============================================================================
// rstm.h: The C interface that $(RSTM)/libstamp/stm.h maps to.
//=============================================================================

#ifndef RSTM_H
#define RSTM_H

#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef void desc_t;

  /*** create/remove the TM environment */
  void tm_main_startup();
  void tm_start(desc_t** tx, int id);
  void tm_stop(int id);

  /*** mark the ends of a transaction */
  void tm_begin_tx(desc_t* tx, jmp_buf* jmpbuf);
  void tm_end_tx(desc_t* descr);
  void tm_restart(desc_t* descr);

  /*** allocation subsystem */
  void* tm_malloc(desc_t* tx, int sz);
  void tm_free(desc_t* tx, void* addr);

  /*** read instrumentation */
  int tm_read(desc_t* tx, int* addr);
  void* tm_readp(desc_t* tx, void** addr);
  float tm_readf(desc_t* tx, float* addr);

  /*** write instrumentation */
  void tm_write(desc_t* tx, int* addr, int newval);
  void tm_writep(desc_t* tx, void** addr, void* newval);
  void tm_writef(desc_t* tx, float* addr, float newval);

#ifdef __cplusplus
}
#endif

#endif // RSTM_H
