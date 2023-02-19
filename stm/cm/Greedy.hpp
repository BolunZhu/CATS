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

#ifndef __GREEDY_H
#define __GREEDY_H

#include "ContentionManager.hpp"
#include "../support/atomic_ops.h"

namespace stm
{
  /**
   *  Greedy is a blocking CM that uses a shared-memory timestamp to determine age.
   */
  class Greedy : public ContentionManager
  {
    private:
      unsigned long timestamp; // For priority
      bool waiting;

      static volatile unsigned long timeCounter;

      ConflictResolutions shouldAbort(ContentionManager* enemy);

    public:
      Greedy() : waiting(false) { timestamp = fai(&timeCounter); }

      virtual void onBeginTransaction() { waiting = false; }

      virtual void onTransactionCommitted()
      {
          priority = 0;
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

      virtual void onOpenRead() { waiting = false; }
      virtual void onOpenWrite() { waiting = false; }
      virtual void onReOpen() { waiting = false; }
  };
}; // namespace stm

inline stm::ConflictResolutions
stm::Greedy::shouldAbort(ContentionManager* enemy)
{
    Greedy* B = static_cast<Greedy*>(enemy);

    if ((timestamp < B->timestamp) || (B->waiting == true)) {
        // I abort B
        waiting = false;
        return AbortOther;
    }
    else {
        waiting = true;
        return Wait;
    }
}

#endif // __GREEDY_H
