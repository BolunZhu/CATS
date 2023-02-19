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

#ifndef __INEVITABILITY_HPP__
#define __INEVITABILITY_HPP__

#include "atomic_ops.h"
#include "word_based_metadata.hpp"

namespace stm
{
  /*** Empty implementation for supporting STM_INEV_NONE */
  struct InevNone
  {
      /*** there is no metadata, but we keep this for compatibility */
      struct Global { };

      static Global globals;

      /*** Constructor is a nop */
      InevNone() { }

      /*** Query if the transaction is inevitable.  Always returns false. */
      bool isInevitable() { return false; }

      /*** Event methods are all nops */
      void onBeginTx() { }
      void onEndTx() { }
      void onInevCommit() { }

      /*** Try to become inevitable.  Always fails. */
      bool try_inevitable() { return false; }
  };

  /*** Per-thread metadata and methods for supporting GRL inevitability */
  struct InevGRL
  {
      /*** Packet of global metadata; we use a singleton for these */
      struct Global
      {
          /*** token to ensure at most one inevitable transaction at a time */
          padded_unsigned_t inev_token;

          /*** counter of the number of active transactions */
          padded_unsigned_t epoch_size;

          /*** array representing all active transactions */
          padded_unsigned_t epoch[MAX_THREADS];

          /*** constructor just zeroes everything */
          Global()
          {
              inev_token.val = 0;
              epoch_size.val = 0;
              for (int i = 0; i < MAX_THREADS; ++i)
                  epoch[i].val = 0;
          }
      };

    private:
      /*** singleton of all the global metadata we need to use */
      static Global globals;

      /*** this thread's position in the global epoch */
      unsigned epoch_slot;

      /*** this thread's local copy of the epoch */
      unsigned epoch_buff[MAX_THREADS];

      /*** flag to track if this thread is inevitable */
      bool is_inevitable;

      /*** this is how a thread waits on the epoch */
      void WaitForDominatingEpoch()
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

    public:

      /*** Constructor gets a slot in the global epoch */
      InevGRL()
      {
          unsigned u = fai(&globals.epoch_size.val);
          epoch_slot = u;
          for (int i = 0; i < MAX_THREADS; i++)
              epoch_buff[i] = 0;
          is_inevitable = false;
      }

      /*** Getter to query the is_inevitable flag */
      bool isInevitable() { return is_inevitable; }

      /*** At begin time, block if anyone is inevitable */
      void onBeginTx()
      {
          // announce that I'm starting a transaction
          while (true) {
              globals.epoch[epoch_slot].val++;
              WBR; // write to epoch before reading inev_token
              unsigned old = globals.inev_token.val;
              // if nobody is inevitable, we're done
              if ((old & 1) == 0)
                  return;
              // someone is inevitable.  They may be waiting on me, so back
              // out, wait a little while, and try again
              globals.epoch[epoch_slot].val--;
              spin128();
          }
      }

      /*** At end time, announce that I'm done with this transaction */
      void onEndTx() { globals.epoch[epoch_slot].val++; }

      /**
       *  Try to become inevitable.  The return value is a bool so that we can
       *  try, fail, and take the appropriate action.  This means that 'must be
       *  inevitable' must be implemented in user code for now, but it also
       *  means that we can use inevitability to accelerate worklist
       *  applications.
       */
      bool try_inevitable()
      {
          // return if this tx is already inevitable
          if (is_inevitable)
              return true;

          // Attempt to increment the token from even to odd, and return
          // false if we can't
          unsigned old = globals.inev_token.val;
          if ((old & 1) == 1)
              return false; // token is held
          if (!bool_cas(&globals.inev_token.val, old, old+1))
              return false; // missed chance to get the token

          // we're inevitable.  Wait for all active transactions to finish, and
          // then we can progress inevitably
          is_inevitable = true;
          WaitForDominatingEpoch();
          return true;
      }

      /**
       *  When the inevitable transaction commits at its outermost level, we
       *  release the token and set our state to 'not inevitable'
       */
      void onInevCommit()
      {
          globals.inev_token.val++;
          is_inevitable = false;
      }
  };

#ifdef STM_INEV_GRL
  typedef InevGRL InevPolicy;
#elif defined STM_INEV_NONE
  typedef InevNone InevPolicy;
#else
  assert(false);//#error "Invalid Inevitability mechanism"
#endif
}

#endif // __INEVITABILITY_HPP__
