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

#ifndef __ERUPTION_H__
#define __ERUPTION_H__

#include "ContentionManager.hpp"

namespace stm
{
  /**
   *  Eruption transfers priority to the transaction it is blocked behind, to
   *  "push it through" to completion
   */
  class Eruption: public ContentionManager
  {
    private:
      enum Backoff { INTERVAL = 1000 };

      int tries;
      int prio_transferred;

      void onTryOpen(void)
      {
          if (tries > 1)
              nano_sleep(INTERVAL);

          tries++;
      }

      void onOpen(void)
      {
          priority++;
          prio_transferred = 0;
          tries = 1;
      }

    public:
      Eruption() : tries(1), prio_transferred(0) { }

      ConflictResolutions shouldAbort(ContentionManager* enemy);
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

      void givePrio(int prio)
      {
          priority += prio;
      }

      // event methods
      virtual void onContention() { onTryOpen(); }
      virtual void onOpenRead() { onOpen(); }
      virtual void onOpenWrite() { onOpen(); }
      virtual void onTransactionCommitted()
      {
          priority = 0;
          prio_transferred = 0;
      }
  };
}; // namespace stm

// get permission to abort enemy
inline stm::ConflictResolutions
stm::Eruption::shouldAbort(ContentionManager* enemy)
{
    Eruption* e;

    if (!enemy)
        return AbortOther;

    if ((tries + priority) > enemy->getPriority())
        return AbortOther;

    e = static_cast<Eruption*>(enemy);

    if (e) {
        e->givePrio(priority - prio_transferred);
        prio_transferred = priority;
    }

    return Wait;
}

#endif // __ERUPTION_H__
