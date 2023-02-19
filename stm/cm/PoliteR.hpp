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

#ifndef POLITER_H__
#define POLITER_H__

#include "ContentionManager.hpp"
#include "../support/hrtime.h"

#ifdef _MSC_VER
#include "../../alt-license/rand_r.h"
#else
#include <stdlib.h>
#endif

namespace stm
{
  /**
   *  PoliteR is a simple nonblocking CM that uses bounded randomized
   *  exponential backoff
   */
  class PoliteR: public ContentionManager
  {
      int rexp;

      // randomized exponential backoff interface; shared with all
      // descendants
    protected:
      enum Backoff {
#ifdef __sparc__
          MIN_BACKOFF = 7,
          MAX_BACKOFF = 28,
#else
          MIN_BACKOFF = 4,
          MAX_BACKOFF = 16,
#endif
          MAX_BACKOFF_RETRIES = MAX_BACKOFF - MIN_BACKOFF + 1
      };

      // how many times have we backed off without opening anything
      int tries;

      // seed for randomized exponential backoff
      unsigned int seed;

      // randomized exponential backoff
      void backoff()
      {
          if (tries > 0 && tries <= MAX_BACKOFF_RETRIES) {
              // what time is it now
              unsigned long long starttime = getElapsedTime();

              // how long should we wait (random)
              unsigned long delay = rand_r(&seed);
              delay = delay % (1 << (tries + MIN_BACKOFF));

              // spin until /delay/ nanoseconds have passed.  We can do
              // whatever we want in the spin, as long as it doesn't have an
              // impact on other threads.  By using getElapsedTime, we
              // shouldn't spin unnecessarily after being swapped out and
              // back in.
              unsigned long long endtime;
              do {
                  endtime = getElapsedTime();
              } while (endtime < starttime + delay);
          }
          tries++;
      }

    private:
      // every time we Open something, reset the try counter
      void onOpen() { tries = 0; }

    public:
      PoliteR() : rexp(64), tries(0), seed(0) { }

      // request permission to abort enemy tx
      virtual ConflictResolutions onWAW(ContentionManager*)
      {
          if (tries > MAX_BACKOFF_RETRIES)
              return AbortOther;
          return Wait;
      }
      virtual ConflictResolutions onRAW(ContentionManager*)
      {
          if (tries > MAX_BACKOFF_RETRIES)
              return AbortOther;
          return Wait;
      }
      virtual ConflictResolutions onWAR(ContentionManager*)
      {
          // livelocks otherwise
          return AbortOther;
      }

      // event methods
      virtual void onContention() { backoff(); }

      virtual void onOpenRead()  { onOpen(); }
      virtual void onOpenWrite() { onOpen(); }
      virtual void onReOpen()    { onOpen(); }

      virtual void onTransactionAborted()
      {
          rexp <<=2;
          nano_sleep(rexp);
      }

      virtual void onTransactionCommitted() { rexp = 64; }
  };
}; // namespace stm

#endif // POLITER_H__
