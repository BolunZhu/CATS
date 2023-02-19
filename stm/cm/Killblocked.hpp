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

#ifndef __KILLBLOCKED_H__
#define __KILLBLOCKED_H__

#include "ContentionManager.hpp"

namespace stm
{
  /**
   *  Killblocked implements linear backoff with a defunct mechanism.
   */
  class Killblocked: public ContentionManager
  {
    private:
      enum Backoff { INTERVAL = 1000, MAX_TRIES = 16 };

      int tries;
      bool blocked;

      // once we open the object, become unblocked
      void onOpen(void)
      {
          blocked = false;
          tries = 0;
      }

      ConflictResolutions shouldAbort(ContentionManager* enemy);

    public:
      Killblocked() : tries(0), blocked(false) { }

      bool IsBlocked(void) { return blocked; }

      // request permission to abort enemy tx
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
      virtual void onContention()
      {
          blocked = true;
          nano_sleep(INTERVAL);
          tries++;
      }

      virtual void onOpenRead()  { onOpen(); }
      virtual void onOpenWrite() { onOpen(); }
      virtual void onReOpen()    { onOpen(); }
  };
}; // namespace stm

// get permission to abort an enemy
inline stm::ConflictResolutions
stm::Killblocked::shouldAbort(ContentionManager* enemy)
{
    Killblocked* k = static_cast<Killblocked*>(enemy);

    if (tries > MAX_TRIES)
        return AbortOther;
    else if (k && k->IsBlocked())
        return AbortOther;

    return Wait;
}

#endif // __KILLBLOCKED_H__
