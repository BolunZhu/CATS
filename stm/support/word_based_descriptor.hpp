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

#ifndef WBDESCRIPTOR_H__
#define WBDESCRIPTOR_H__
#include <setjmp.h>
#include "defs.hpp"
#include "word_based_metadata.hpp"
#include "word_based_writeset.hpp"
#include "MiniVector.hpp"

namespace stm
{
  class WBTxThread
  {
    protected: // typedefs
      typedef MiniVector<wlog_t>  ValueList;
      typedef WriteSet            ValueHash;
#ifdef STM_USE_MINIVECTOR_WRITESET
      typedef ValueList RedoLog;
#else
      typedef ValueHash RedoLog;
#endif

    protected: // fields

      /*** transaction state can be ACTIVE, COMMITTED, or ABORTED */
      volatile unsigned long tx_state;

    protected: // methods

      /*** simple constructor: start in nontransactional state */
      WBTxThread() : tx_state(COMMITTED) { }

      /*** Allows == comparison of Descriptors. */
      bool operator==(const WBTxThread& rhs) const { return (rhs == *this); }

      /*** Allows != comparison of Descriptors. */
      bool operator!=(const WBTxThread& rhs) const { return (rhs != *this); }

      /*** hash function for a 32-bit write filter */
      static unsigned simplehash(void* val)
      {
          return (((unsigned)val)>>5) & 0x1f;
      }

    public: // fields

      /*** is the thread in a transactional context> */
      bool isTransactional() { return tx_state != COMMITTED; }
  }; // class WBTxThread

  /*** Orec-Based word-based STMs all share this code */
  class OrecWBTxThread : public WBTxThread
  {
    protected: // typedefs

      typedef stm::owner_version_t<OrecWBTxThread> owner_version_t;
      typedef stm::orec_t<OrecWBTxThread> orec_t;
      typedef MiniVector<orec_t*> OrecList;

    protected: // orec support

      /*** Specify the number of orecs in the global array. */
      static const unsigned NUM_STRIPES = 1048576;

      /*** declare the table of orecs */
      static volatile orec_t orecs[NUM_STRIPES];

      /*** map addresses to orec table entries */
      static volatile orec_t* get_orec(void* addr)
      {
          unsigned index = reinterpret_cast<unsigned>(addr);
          return &orecs[(index>>3) % NUM_STRIPES];
      }

  }; // class OrecWBTxThread

} // namespace stm

#endif // WBDESCRIPTOR_H__
