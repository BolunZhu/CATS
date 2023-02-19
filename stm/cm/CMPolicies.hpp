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

#ifndef __CMPOLICIES_H__
#define __CMPOLICIES_H__

#include "ContentionManager.hpp"

namespace stm
{
  /**
   *  Policy that prefers to use a static CM with all inlined calls and no
   *  vtable overheads, but that lets you specify a different CM at run time
   *  if that's what you really want.
   */
  class HybridCMPolicy
  {
      /**
       *  Flag for deciding which CM to use.
       */
      const bool static_flag;

      /**
       *  Static CM.  All calls get inlined, but you can't change your CM at
       *  run time.  Used if use_static_cm is true.
       */
      STM_DEFAULT_CM staticCM;

      /**
       *  Dynamic contention manager (used only if use_static_cm is
       *  false).  Has big vtable overhead, but you can pick you CM at
       *  run time.
       */
      ContentionManager* dynamicCM;

    public:

      /**
       *  Set up the hybrid policy: if the bool flag is false, then get a CM
       *  from the factory.
       */
      HybridCMPolicy(bool static_cm, std::string dynamic_cm)
          : static_flag(static_cm), staticCM()
      {
          if (!static_flag)
              dynamicCM = Factory(dynamic_cm);
      }

      /**
       *  return a contention manager (used by a CM to get another
       *  descriptor's CM)
       */
      ContentionManager* getCM()
      {
          if (static_flag) return &staticCM;
          else             return dynamicCM;
      }

      ///  Wrapper around onBeginTransaction
      void onBeginTx()
      {
          if (static_flag) staticCM.onBeginTransaction();
          else             dynamicCM->onBeginTransaction();
      }

      ///  Wrapper for onTryCommitTransaction
      void onTryCommitTx()
      {
          if (static_flag) staticCM.onTryCommitTransaction();
          else             dynamicCM->onTryCommitTransaction();
      }

      ///  Wrapper for onTransactionCommitted
      void onTxCommitted()
      {
          if (static_flag) staticCM.onTransactionCommitted();
          else             dynamicCM->onTransactionCommitted();
      }

      ///  Wrapper for onTransactionAborted
      void onTxAborted()
      {
          if (static_flag) staticCM.onTransactionAborted();
          else             dynamicCM->onTransactionAborted();
      }

      ///  Wrapper for onContention
      void onContention()
      {
          if (static_flag) staticCM.onContention();
          else             dynamicCM->onContention();
      }

      ///  Wrapper for onOpenRead
      void onOpenRead()
      {
          if (static_flag) staticCM.onOpenRead();
          else             dynamicCM->onOpenRead();
      }

      ///  Wrapper for onOpenWrite
      void onOpenWrite()
      {
          if (static_flag) staticCM.onOpenWrite();
          else             dynamicCM->onOpenWrite();
      }

      ///  Wrapper for onReOpen
      void onReOpen()
      {
          if (static_flag) staticCM.onReOpen();
          else             dynamicCM->onReOpen();
      }

      ///  Wrapper for onRAW
      ConflictResolutions onRAW(ContentionManager* enemy)
      {
          if (static_flag) return staticCM.onRAW(enemy);
          else             return dynamicCM->onRAW(enemy);
      }
      ///  Wrapper for onWAR
      ConflictResolutions onWAR(ContentionManager* enemy)
      {
          if (static_flag) return staticCM.onWAR(enemy);
          else             return dynamicCM->onWAR(enemy);
      }
      ///  Wrapper for onWAW
      ConflictResolutions onWAW(ContentionManager* enemy)
      {
          if (static_flag) return staticCM.onWAW(enemy);
          else             return dynamicCM->onWAW(enemy);
      }
  };

  /**
   *  Policy that ignores user input and always just uses a static CM
   */
  class PureStaticCMPolicy
  {
      ///  Static CM to use at all times
      STM_DEFAULT_CM staticCM;

    public:

      ///  Set up the PureStatic policy by constructing staticCM
      PureStaticCMPolicy(bool b, std::string s) : staticCM() { }

      /**
       *  return a contention manager (used by a CM to get another
       *  descriptor's CM)
       */
      ContentionManager* getCM() { return &staticCM; }

      ///  Wrapper around onBeginTransaction
      void onBeginTx() { staticCM.onBeginTransaction(); }

      ///  Wrapper for onTryCommitTransaction
      void onTryCommitTx() { staticCM.onTryCommitTransaction(); }

      ///  Wrapper for onTransactionCommitted
      void onTxCommitted() { staticCM.onTransactionCommitted(); }

      ///  Wrapper for onTransactionAborted
      void onTxAborted() { staticCM.onTransactionAborted(); }

      ///  Wrapper for onContention
      void onContention() { staticCM.onContention(); }

      ///  Wrapper for onOpenRead
      void onOpenRead() { staticCM.onOpenRead(); }

      ///  Wrapper for onOpenWrite
      void onOpenWrite() { staticCM.onOpenWrite(); }

      ///  Wrapper for onReOpen
      void onReOpen() { staticCM.onReOpen(); }

      ///  Wrapper for onRAW
      ConflictResolutions onRAW(ContentionManager* enemy)
      {
          return staticCM.onRAW(enemy);
      }
      ///  Wrapper for onWAR
      ConflictResolutions onWAR(ContentionManager* enemy)
      {
          return staticCM.onWAR(enemy);
      }
      ///  Wrapper for onWAW
      ConflictResolutions onWAW(ContentionManager* enemy)
      {
          return staticCM.onWAW(enemy);
      }
  };

  /**
   *  Policy that always uses a dynamically allocated CM and pays the vtable
   *  overhead.
   */
  class PureDynamicCMPolicy
  {
      /**
       *  Contention manager to use for all calls.  Has big vtable overhead,
       *  but you can pick you CM at run time.
       */
      ContentionManager* dynamicCM;

    public:

      /**
       *  Set up the PureDynamic policy by calling the factory
       */
      PureDynamicCMPolicy(bool b, std::string cm_name)
      {
          dynamicCM = Factory(cm_name);
      }

      /**
       *  return a contention manager (used by a CM to get another descriptor's
       *  CM)
       */
      ContentionManager* getCM() { return dynamicCM; }

      ///  Wrapper around onBeginTransaction
      void onBeginTx() { dynamicCM->onBeginTransaction(); }

      ///  Wrapper for onTryCommitTransaction
      void onTryCommitTx() { dynamicCM->onTryCommitTransaction(); }

      ///  Wrapper for onTransactionCommitted
      void onTxCommitted() { dynamicCM->onTransactionCommitted(); }

      ///  Wrapper for onTransactionAborted
      void onTxAborted() { dynamicCM->onTransactionAborted(); }

      ///  Wrapper for onContention
      void onContention() { dynamicCM->onContention(); }

      ///  Wrapper for onOpenRead
      void onOpenRead() { dynamicCM->onOpenRead(); }

      ///  Wrapper for onOpenWrite
      void onOpenWrite() { dynamicCM->onOpenWrite(); }

      ///  Wrapper for onReOpen
      void onReOpen() { dynamicCM->onReOpen(); }

      ///  Wrapper for onRAW
      ConflictResolutions onRAW(ContentionManager* enemy)
      {
          return dynamicCM->onRAW(enemy);
      }
      ///  Wrapper for onWAR
      ConflictResolutions onWAR(ContentionManager* enemy)
      {
          return dynamicCM->onWAR(enemy);
      }
      ///  Wrapper for onWAW
      ConflictResolutions onWAW(ContentionManager* enemy)
      {
          return dynamicCM->onWAW(enemy);
      }
  };
} // namespace stm
#endif // __CMPOLICIES_H__
