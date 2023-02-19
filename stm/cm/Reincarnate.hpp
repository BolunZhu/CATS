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

#ifndef __REINCARNATE_H__
#define __REINCARNATE_H__

#include "ContentionManager.hpp"
#include "../support/atomic_ops.h"

#ifdef _MSC_VER
#include "../../alt-license/rand_r.h"
#endif

namespace stm
{
  /**
   *  Serializer with exponential backoff and nonblocking guarantees via a
   *  timeout on waiting.  One can think of Greedy as 'resurrect', in that
   *  the Greedy CM doesn't get a new timestamp on abort.  Reincarnate gets a
   *  new timestamp on abort.  So Greedy (when coupled with visible readers)
   *  is starvation-free.  Reincarnate (even when coupled with invisible
   *  readers) is much closer to livelock freedom.  If the workload has a
   *  regular access pattern, this CM should keep everything nonblocking and
   *  should avoid livelock.
   */
  class Reincarnate : public ContentionManager
  {
      unsigned long timestamp; // For priority

      static volatile unsigned long timeCounter;

      enum Backoff {
          MIN_BACKOFF = 7,
          MAX_BACKOFF = 28,
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
              // whatever we want in the spin, as long as it doesn't have
              // an impact on other threads
              unsigned long long endtime;
              do {
                  endtime = getElapsedTime();
              } while (endtime < starttime + delay);
          }
          tries++;
      }

      // every time we Open something, reset the try counter
      void onOpen() { tries = 0; }

      ConflictResolutions shouldAbort(ContentionManager* enemy)
      {
          Reincarnate* B = static_cast<Reincarnate*>(enemy);
          if (timestamp < B->timestamp)
              return AbortOther;
          else
              if (tries > MAX_BACKOFF_RETRIES)
                  return AbortOther;
              else
                  return Wait;
      }

    public:
      Reincarnate() : tries(0), seed((unsigned long)&tries) { }
      virtual void onBeginTransaction()
      {
          tries = 0;
          timestamp = fai(&timeCounter);
      }

      virtual ConflictResolutions onRAW(ContentionManager* e)
      {
          return shouldAbort(e);
      }
      virtual ConflictResolutions onWAR(ContentionManager* e)
      {
          return shouldAbort(e);
      }
      virtual ConflictResolutions onWAW(ContentionManager* e)
      {
          return shouldAbort(e);
      }

      // event methods
      virtual void onContention() { backoff(); }

      virtual void onOpenRead()  { onOpen(); }
      virtual void onOpenWrite() { onOpen(); }
      virtual void onReOpen()    { onOpen(); }
  };
}; // namespace stm

#endif // __REINCARNATE_H__
