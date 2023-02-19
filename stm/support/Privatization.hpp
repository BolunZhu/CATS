///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007, 2008, 2009, 2009
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

#ifndef __PRIVATIZATION_HPP__
#define __PRIVATIZATION_HPP__

#include "atomic_ops.h"
#include "word_based_metadata.hpp"

namespace stm
{
  /*** Empty implementation for supporting STM_PRIV_LOGIC */
  struct PrivLogic
  {
      /*** there is no metadata, but we keep this for compatibility */
      struct Global { };

      static Global globals;

      /*** Constructor is a nop */
      PrivLogic() { }

      /*** Event methods are all nops */
      void onBeginTx() { }
      void onEndTx() { }

      /*** The Fence instruction is a nop */
      void Fence() { }
  };

  /*** Per-thread metadata and methods for supporting TFence privatization */
  struct PrivTFence
  {
      /*** Packet of global metadata; we use a singleton for these */
      struct Global
      {
          /*** counter of the number of active transactions */
          padded_unsigned_t epoch_size;

          /*** array representing all active transactions */
          padded_unsigned_t epoch[MAX_THREADS];

          /*** constructor just zeroes everything */
          Global()
          {
              epoch_size.val = 0;
              for (int i = 0; i < MAX_THREADS; ++i)
                  epoch[i].val = 0;
          }
      };

      /*** singleton of all the global metadata we need to use */
      static Global globals;

      /*** this thread's position in the global epoch */
      unsigned epoch_slot;

      /*** this thread's local copy of the epoch */
      unsigned epoch_buff[MAX_THREADS];

    public:
      /*** Constructor gets a slot in the global epoch */
      PrivTFence()
      {
          unsigned u = fai(&globals.epoch_size.val);
          epoch_slot = u;
          for (int i = 0; i < MAX_THREADS; i++)
              epoch_buff[i] = 0;
      }

      /*** At begin time, increment my epoch */
      void onBeginTx() { globals.epoch[epoch_slot].val++; }

      /*** At commit/abort time, increment my epoch */
      void onEndTx() { globals.epoch[epoch_slot].val++; }

      /*** this is how a thread waits on the epoch */
      void Fence()
      {
          // copy the current epoch
          unsigned num_txns = globals.epoch_size.val;
          for (unsigned i = 0; i < num_txns; i++)
              epoch_buff[i] = globals.epoch[i].val;
          // wait for all slots to be even
          for (unsigned i = 0; i < num_txns; i++)
              if ((i != epoch_slot) && ((epoch_buff[i] & 1) == 1))
                  while (globals.epoch[i].val == epoch_buff[i])
                      spin64();
      }
  };

#ifdef STM_PRIV_LOGIC
  typedef PrivLogic PrivPolicy;
#elif defined STM_PRIV_TFENCE
  typedef PrivTFence PrivPolicy;
#else
  assert(false);//#error "Invalid Privatization mechanism"
#endif
}

#endif // __PRIVATIZATION_HPP__
