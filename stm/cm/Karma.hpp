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

#ifndef __KARMA_H__
#define __KARMA_H__

#include "ContentionManager.hpp"

namespace stm
{
  /**
   * Karma is a nonblocking CM that uses number of objects opened to
   * represent priority
   */
  class Karma: public ContentionManager
  {
    private:
      enum Backoff { INTERVAL = 1000 };

      int tries;

      // every time we Try to Open something, backoff once more and
      // increment the try counter
      void onTryOpen(void)
      {
          if (tries > 1)
              nano_sleep(INTERVAL);

          tries++;
      }

      // every time we Open something, reset the try counter, increase
      // the karma
      void onOpen(void)
      {
          priority++;
          tries = 1;
      }

      // request permission to abort enemies
      ConflictResolutions shouldAbort(ContentionManager* enemy)
      {
          if (!enemy)
              return AbortOther;
          if ((tries + priority) > enemy->getPriority())
              return AbortOther;
          return Wait;
      }

    public:
      // constructor
      Karma() : tries(1) { }

      virtual ConflictResolutions onRAW(ContentionManager* e)
      {
          return shouldAbort(e);
      }
      virtual ConflictResolutions onWAR(ContentionManager* e)
      {
          return AbortOther;
      }
      virtual ConflictResolutions onWAW(ContentionManager* e)
      {
          return shouldAbort(e);
      }

      // event methods
      virtual void onReOpen() { tries = 1; }
      virtual void onContention() { onTryOpen(); }
      virtual void onOpenRead() { onOpen(); }
      virtual void onOpenWrite() { onOpen(); }
      virtual void onTransactionCommitted() { priority = 0; }
  };
}; // namespace stm

#endif // __KARMA_H__
