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

#ifndef __JUSTICE_H__
#define __JUSTICE_H__

#include "ContentionManager.hpp"
#include "Polite.hpp"

namespace stm
{
  /**
   * Justice is a Polka variant with different read, write, and try weights.
   * Its presence more-or-less obviates Whpolka.
   */
  class Justice : public Polite
  {
      enum Weights {
          READWEIGHT  = 4,
          WRITEWEIGHT = 16,
          TRYWEIGHT   = 1
      };

    protected:
      unsigned long reads;
      unsigned long writes;

      // query the enemy cm to get its priority, return true if enemy's
      // priority is lower than mine.
      ConflictResolutions shouldAbort(ContentionManager* enemy)
      {
          Justice* e = static_cast<Justice*>(enemy);
          if (jprio() > e->jprio())
              return AbortOther;
          return Wait;
      }

    public:
      // everything in the Justice ctor is handled by Polite() and
      // ContentionManager()
      Justice() : reads(0), writes(0) { }

      virtual ConflictResolutions onRAW(ContentionManager* enemy)
      {
          return shouldAbort(enemy);
      }
      virtual ConflictResolutions onWAR(ContentionManager* enemy)
      {
          return AbortOther;
      }
      virtual ConflictResolutions onWAW(ContentionManager* enemy)
      {
          return shouldAbort(enemy);
      }

      unsigned long jprio()
      {
          unsigned long mass =
              reads*READWEIGHT + writes*WRITEWEIGHT + tries*TRYWEIGHT;
          return mass;
      }

      // event methods
      virtual void onReOpen()     { tries = 0; }
      virtual void onOpenRead()   { reads++; tries = 0; }
      virtual void onOpenWrite()  { writes++; tries = 0; }

      virtual void onTransactionCommitted()
      {
          reads = 0;
          writes = 0;
          priority = 0;
      }
  };
}; // namespace stm

#endif // __JUSTICE_H__
