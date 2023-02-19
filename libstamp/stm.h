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

//=============================================================================
// stm.h: The RSTM specific implementation of STAMP's STM_* macro interface.
//=============================================================================

#ifndef STM_H
#define STM_H

#include "rstm.h"

#define STM_THREAD_T             void
#define STM_SELF                 tm_descriptor
#define STM_MALLOC(size)         tm_malloc(STM_SELF, size)
#define STM_FREE(ptr)            tm_free(STM_SELF, ptr)
#define STM_STARTUP(numThread)   tm_main_startup()
#define STM_SHUTDOWN()
#define STM_NEW_THREAD()         0
#define STM_INIT_THREAD(t, id)   tm_start(&t, thread_getId())
#define STM_FREE_THREAD(t)       tm_stop(thread_getId())
#define STM_RESTART()            tm_restart(STM_SELF)

#define STM_READ(addr)           tm_read(STM_SELF, (void*) &addr)
#define STM_READ_P(addr)         tm_readp(STM_SELF, (void*) &addr)
#define STM_READ_F(addr)         tm_readf(STM_SELF, (void*) &addr)

#define STM_WRITE(addr, val)     tm_write(STM_SELF, (void*) &addr, (int) val)
#define STM_WRITE_P(addr, val)   tm_writep(STM_SELF, (void*) &addr, (void*) val)
#define STM_WRITE_F(addr, val)   tm_writef(STM_SELF, (void*) &addr, (float) val)

#define STM_LOCAL_WRITE(var, val)   ({var = val; var;})
#define STM_LOCAL_WRITE_F(var, val) ({var = val; var;})
#define STM_LOCAL_WRITE_P(var, val) ({var = val; var;})

#define STM_BEGIN_WR()                          \
  {                                             \
    while (1) {                                 \
      jmp_buf jmpbuf_;                          \
      setjmp(jmpbuf_);                          \
      tm_begin_tx(STM_SELF, &jmpbuf_);          \
      {

#define STM_BEGIN_RD() STM_BEGIN_WR()

#define STM_END()                               \
      }                                         \
      tm_end_tx(STM_SELF);                      \
      break;                                    \
    }                                           \
  }

#endif // STM_H
