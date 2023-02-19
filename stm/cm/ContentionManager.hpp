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

#ifndef __CONTENTIONMANAGER_H__
#define __CONTENTIONMANAGER_H__

#include <string>
#include "../support/atomic_ops.h"
#include "../support/hrtime.h"

namespace stm
{
  enum ConflictResolutions { AbortSelf, AbortOther, Wait };

  class ContentionManager
  {
    protected:
      int priority;
    public:
      ContentionManager() : priority(0) { }
      int getPriority() { return priority; }

      ////////////////////////////////////////
      // Transaction-level events
      virtual void onBeginTransaction() { }
      virtual void onTryCommitTransaction() { }
      virtual void onTransactionCommitted() { priority = 0; }
      virtual void onTransactionAborted() { }

      ////////////////////////////////////////
      // Object-level events
      virtual void onContention() { }
      virtual void onOpenRead() { }
      virtual void onOpenWrite() { }
      virtual void onReOpen() { }

      ////////////////////////////////////////
      // Conflict Event methods
      virtual ConflictResolutions onRAW(ContentionManager* enemy) = 0;
      virtual ConflictResolutions onWAR(ContentionManager* enemy) = 0;
      virtual ConflictResolutions onWAW(ContentionManager* enemy) = 0;

      virtual ~ContentionManager() { }
  };

  // create a contention manager
  ContentionManager* Factory(std::string cm_type);

  // wait by executing a bunch of nops in a loop (not preemption
  // tolerant)
  inline void nano_sleep(unsigned long nops)
  {
      for (unsigned i = 0; i < nops; i++)
          nop();
  }
} // namespace stm

#endif  // __CONTENTIONMANAGER_H__
