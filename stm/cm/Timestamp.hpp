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

#ifndef __TIMESTAMP_H__
#define __TIMESTAMP_H__

#ifdef _MSC_VER
#include <time.h>
#include <winsock.h>
#else
#include <sys/time.h>
#endif

#include "ContentionManager.hpp"

namespace stm
{
  /**
   *  Timestamp combines a notion of priority based on time (in this case, OS
   *  time) with a defunct mechanism (don't wait on someone who is waiting)
   *  and bounded delay
   */
  class Timestamp: public ContentionManager
  {
    private:
      enum Backoff { INTERVAL = 1000, MAX_TRIES = 8 };

      int tries;
      int max_tries;
      time_t stamp;
      bool defunct;

      void onOpen(void)
      {
          defunct = false;
          tries = 0;
          max_tries = MAX_TRIES;
      }

      ConflictResolutions shouldAbort(ContentionManager* enemy);

    public:
      Timestamp() : tries(0), max_tries(MAX_TRIES), defunct(false) { }

      time_t GetTimestamp() { return stamp; }
      void SetDefunct() { defunct = true; }
      bool GetDefunct() { return defunct; }

      virtual void onContention()
      {
          defunct = false;
          nano_sleep(INTERVAL);
          tries++;
      }

      virtual void onOpenRead() { onOpen(); }
      virtual void onOpenWrite() { onOpen(); }
      virtual void onTransactionCommitted() { defunct = false; }
      virtual void onTryCommitTransaction() { defunct = false; }
      virtual void onTransactionAborted() { defunct = false; }
      virtual void onBeginTransaction();

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
  };
}; // namespace stm

inline stm::ConflictResolutions
stm::Timestamp::shouldAbort(ContentionManager* enemy)
{
    Timestamp* t = static_cast<Timestamp*>(enemy);
    defunct = false;

    if (!t)
        return AbortOther;

    // always abort younger transactions
    if (t->GetTimestamp() <= stamp)
        return AbortOther;

    // if it's been a while, mark the enemy defunct
    if (tries == (max_tries/2)) {
        t->SetDefunct();
    }
    // at some point, finally give up and abort the enemy
    else if (tries == max_tries) {
        return AbortOther;
    }
    // if the enemy was marked defunct and isn't anymore, then we reset a bit
    else if ((tries > (max_tries/2)) && (t->GetDefunct() == false)) {
        tries = 2;
        max_tries *= 2;
    }

    return Wait;
}

inline void stm::Timestamp::onBeginTransaction()
{
    struct timeval t;

    defunct = false;
    gettimeofday(&t, NULL);
    stamp = t.tv_sec;
}

#endif // __TIMESTAMP_H__
