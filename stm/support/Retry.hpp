///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2008, 2009
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

#ifndef RETRY_HPP__
#define RETRY_HPP__

#include "config.h"
#include "Bloom.hpp"
#include "TokenManager.hpp"

#ifdef _MSC_VER
#include <windows.h>
#pragma comment(lib, "winmm.lib") // for Sleep
#else
#include <semaphore.h>
#include <unistd.h> // for usleep
#endif

/*** forward declaration for VisReadRetry */
namespace rstm { struct SharedHandle; }

namespace stm
{
  /**
   *  The usleep and semaphore interfaces differ between *nix and Win32.  This
   *  class encapsulates the differences, so that we don't have to re-implement
   *  the code in each of our retry mechanisms.
   */
  struct AbstractSchedulerInterface
  {
      /***  The semaphore that the owner of this entry is sleeping on */
#ifdef _MSC_VER
      HANDLE sem;
#else
      sem_t sem;
#endif

      /***  Wake up someone sleeping on the semaphore */
      void post()
      {
#ifdef _MSC_VER
          ReleaseSemaphore(sem, 1, NULL);
#else
          sem_post(&sem);
#endif
      }

      /***  Block this thread (via OS) until someone posts on the semaphore */
      void wait()
      {
#ifdef _MSC_VER
          WaitForSingleObject(sem, INFINITE);
#else
          sem_wait(&sem);
#endif
      }

      /*** reset the semaphore if it had multiple posts */
      void reset_sem()
      {
#ifdef _MSC_VER
          while (WaitForSingleObject(sem, 0) != WAIT_OBJECT_0);
#else
          while (!sem_trywait(&sem));
#endif
      }

      /*** Simple constructor: initialize the semaphore */
      AbstractSchedulerInterface()
      {
#ifdef _MSC_VER
          // lower the granularity of the sleep() method to 1 ms, which is the
          // best we can get easily in Win32
          timeBeginPeriod(1);
          // set up the semaphore
          sem = CreateSemaphore(NULL, 0, 128, NULL);
#else
          sem_init(&sem, 0, 0);
#endif
      }

      /*** destructor destroys the semaphore */
      ~AbstractSchedulerInterface()
      {
#ifdef _MSC_VER
          CloseHandle(sem);
#else
          sem_destroy(&sem);
#endif
      }

      /*** sleep for a number of microseconds */
      static void sleep(unsigned usecs)
      {
#ifdef _MSC_VER
          unsigned time = (usecs < 1000) ? 1 : usecs / 1000;
          Sleep(time);
#else
          usleep(usecs);
#endif
      }
  };

  /**
   *  Singleton storing the list of sleeping bloom filters, and implementing
   *  the code for interacting with that list.
   */
  class BloomRetry
  {
    public:
      /*** Each retrying transaction is represented by one of these. */
      class RetryHandle : public AbstractSchedulerInterface
      {
          friend class BloomRetry;

          /**
           * Filter representing all addresses (or objects) read
           *
           * for now we'll use 1Kbit filters with 3 hash functions, but this
           * should become a compile-time parameter eventually
           */
          Bloom<1024, 3> filter;

          /*** status counter.  even means this filter is inactive */
          volatile unsigned long status;

        public:
          /*** reset the filter */
          void reset() { filter.reset(); }

          /*** Add something to our filter. */
          void insert(void* ptr) { filter.insert((unsigned long)ptr); }

          /*** Simple constructor: initialize the semaphore */
          RetryHandle() : AbstractSchedulerInterface(), filter(), status(0) { }

          /*** destructor destroys the semaphore */
          ~RetryHandle() { }
      };

      /**
       *  For uniformity, expose an empty class that can be added to the header
       *  of shared objects
       */
      struct PerObjectRetryMetadata
      {
          /*** constructor must take an unsigned long long */
          PerObjectRetryMetadata(unsigned long long i) { }
      };

      /*** hard-coded max threads (for now) */
      static const unsigned long MAX_RETRY_THREADS = 64;

      /**
       *  TokenManager to store up to 64 thread RetryHandle objects
       *
       *  note that we could make do with a simple list if returning entries
       *  isn't an issue (we don't return them right now anyway)
       */
      stm::TokenManager<RetryHandle> handles;

      /*** fast-path test if any retryers in the list */
      volatile unsigned long thread_count;

      /***  Initialize by zeroing the list of retrying transactions */
      BloomRetry() :  handles(MAX_RETRY_THREADS), thread_count(0) { }

      /**
       *  When a transaction commits, it needs to check if there are waiting
       *  transactions, and if so it needs to wake them up if any entry in its
       *  write set hits in the waiting transaction's filter
       *
       *  NB: there is a bit of a hack here.  We assume that the WS collection
       *  stores things that have a field called 'shared', and that that field
       *  is the address we care about.
       */
      template <class WS1, class WS2>
      void onCommit(WS1 ws1, WS2 ws2)
      {
          // read the head, and if it is null we can just return
          if (thread_count == 0)
              return;

          // check each entry in the filter list and see if we should wake it
          for (int i = 0; i < handles.get_max(); i++) {
              // skip this slot in the array if it doesn't hold a valid filter
              RetryHandle* h = handles.lookup(i);
              if (!h)
                  continue;

              // skip this filter if it isn't active
              unsigned long filter_seq = h->status;
              if ((filter_seq & 1) != 1)
                  continue;

              bool activated = false; // track if filter was activated
              // go through the first write set
              for (typename WS1::iterator i1 = ws1.begin();
                   i1 != ws1.end(); ++i1)
              {
                  // stop if filter changed
                  if (h->status != filter_seq) {
                      activated = true;
                      break;
                  }
                  // should I wake this?
                  if (h->filter.lookup((unsigned long)i1->shared)) {
                      h->post();
                      activated = true;
                      break;
                  }
              }

              // if this filter was activated, move to next filter
              if (activated)
                  continue;

              // go through second write set
              for (typename WS2::iterator i2 = ws2.begin();
                   i2 != ws2.end(); ++i2)
              {
                  // stop if filter changed
                  if (h->status != filter_seq)
                      break;
                  // should I wake this
                  if (h->filter.lookup((unsigned long)i2->shared)) {
                      h->post();
                      break;
                  }
              }
          }
      }

      /**
       *  On Transaction Retry, reset the semaphore and mark this filter as
       *  active.  The filter must be configured before calling this
       */
      void beginRetry(RetryHandle* handle)
      {
          // reset the semaphore
          handle->reset_sem();

          // mark that there is a retryer to test against
          fai(&thread_count);

          // mark handle as even
          handle->status++;
      }

      /*** clean up without yielding the CPU */
      void cancelRetry(RetryHandle* handle)
      {
          // decrement count of retryers
          faa(&thread_count, -1);

          // uninit my filter
          handle->status++;
      }

      /*** Yield the CPU, then clean up */
      void endRetry(RetryHandle* handle)
      {
          // sleep
          handle->wait();

          // unset filter
          cancelRetry(handle);
      }

      /**
       *  At Descriptor construction time, pass a handle to the manager and get
       *  an ID, which will then be used for the rest of time
       */
      void init_thread(RetryHandle* handle)
      {
          int id = handles.get_token(handle);
          assert((id < 64) && (id >= 0));
      }
  };

  /**
   *  Singleton for sleep-based retry doesn't actually have any global
   *  variables, but we implement retry in this class to keep the interface
   *  consistent with other mechanisms.
   */
  class SleepRetry
  {
      /*** this should be a compiler option eventually.  usecs to sleep */
      static const unsigned long SLEEP_AMOUNT = 50;

    public:
      /*** Each retrying transaction is represented by one of these. */
      class RetryHandle : public AbstractSchedulerInterface
      {
          friend class SleepRetry;

        public:
          /*** initialize all fields */
          RetryHandle() : AbstractSchedulerInterface() { }

          /*** destructor forwards to parent class to destroy the semaphore */
          ~RetryHandle() { }
      };

      /**
       *  For uniformity, expose an empty class that can be added to the header
       *  of shared objects
       */
      struct PerObjectRetryMetadata
      {
          /*** constructor must take an unsigned long long */
          PerObjectRetryMetadata(unsigned long long i) { }
      };

      /*** Initialize is a nop since there are no fields */
      SleepRetry() { }

      /*** On Transaction Commit, call this for uniformity (but do nothing) */
      template <class WS1, class WS2>
      void onCommit(WS1 ws1, WS2 ws2) { }

      /*** On Transaction Retry, do nothing */
      void beginRetry(RetryHandle* handle) { }

      /*** clean up without yielding the CPU */
      void cancelRetry(RetryHandle* handle) { }

      /*** Yield the CPU, then clean up */
      void endRetry(RetryHandle* handle) { handle->sleep(SLEEP_AMOUNT); }

      /*** no initialization required */
      void init_thread(RetryHandle* handle) { }
  };

  /**
   *  Singleton for managing retry bits, and for implementing the code for
   *  vis-read retry.
   *
   *  The template type should always be rstm::SharedHandle
   */
  template <class SHAREDHANDLE>
  class VisReadRetry
  {
    public:
      /*** Each retrying transaction is represented by one of these. */
      class RetryHandle : public AbstractSchedulerInterface
      {
          friend class VisReadRetry;

          /*** refers to the bit associated with this handle */
          int id;

          /*** store all of the objects whose retry bits we set here */
          MiniVector<SHAREDHANDLE*> markedObjects;

          /*** install into retry bit */
          template <class T>
          void installRetry(T* shared)
          {
              unsigned long long flag = (1ULL << id);
              unsigned long long oldval, newval;
              do {
                  oldval = shared->m_retryers;
                  // casX takes ull* for its old and new, so we need
                  newval = oldval | flag;
              } while (!casX(&shared->m_retryers, &oldval, &newval));
          }

          /*** uninstall retry bit */
          template <class T>
          bool removeRetry(T* shared)
          {
              unsigned long long flag = (1ULL << id);
              unsigned long long expected;
              unsigned long long n;

              // exit immediately if bit not set
              if (!(shared->m_retryers & flag))
                  return true;
              do {
                  expected = shared->m_retryers;
                  n = expected & ~flag;
              } while (!casX(&shared->m_retryers, &expected, &n));
              return false;
          }

          /*** simple getter and setter for the id */
          void set_id(int i) { id = i; assert((i < 64) && (i >= 0)); }
        public:

          /*** unset all retry bits and reset the list */
          void reset()
          {
              typename MiniVector<SHAREDHANDLE*>::iterator i;
              for (i = markedObjects.begin(); i != markedObjects.end(); ++i)
                  removeRetry(*i);
              markedObjects.reset();
          }

          /**
           * set a retry bit and log it for later
           *
           * NB: we use void* to remain compatible with other mechanisms
           */
          void insert(SHAREDHANDLE* ptr)
          {
              installRetry(ptr);
              markedObjects.insert(ptr);
          }

          /*** Simple constructor: initialize the semaphore */
          RetryHandle()
              : AbstractSchedulerInterface(), id(-1), markedObjects(128)
          { }

          /*** destructor destroys the semaphore */
          ~RetryHandle() { }
      };

      /**
       *  Inject this type into shared handles to provide space for marking
       *  retryer transactions.
       */
      typedef unsigned long long PerObjectRetryMetadata;

      /*** hard-coded max threads (for now) */
      static const unsigned long MAX_RETRY_THREADS = 64;

      /**
       *  TokenManager to map up to 64 thread RetryHandle objects to the 64
       *  retry bits.  we need to return bit reservations eventually
       */
      stm::TokenManager<RetryHandle> handles;

      /***  Initialize the global RetryManager by setting its tokenmanager */
      VisReadRetry() : handles(MAX_RETRY_THREADS) { }

      /**
       * When a transaction commits, it needs to check if any retry bits are
       * set for any object in its write set
       *
       * NB: we assume the type of m_retryers, and we assume that the WS
       * collection stores things that have a field called 'shared'
       */
      template <class WS1, class WS2>
      void onCommit(WS1 ws1, WS2 ws2)
      {
          unsigned long long bmp = 0;
          // get all retry bitmaps from eager writes and 'or' them into bmp
          for (typename WS1::iterator i = ws1.begin(); i != ws1.end(); ++i)
              bmp |= i->shared->m_retryers;

          // do the same for lazy writes
          for (typename WS2::iterator i = ws2.begin(); i != ws2.end(); ++i)
              bmp |= i->shared->m_retryers;

          // if bmp is 0, we're done
          if (!bmp)
              return;

          // time to wake everyone in bmp
          unsigned long long flag = 1;
          for (unsigned int idx = 0; idx < (sizeof(bmp) * 8); ++idx) {
              if (bmp & flag)
                  handles.lookup(idx)->post();
              flag <<= 1;
          }
      }

      /**
       *  On Transaction Retry, just reset the semaphore.  No bits should be
       *  set before calling this
       *
       *  Eventually we will want to get a new ID here and return it later, but
       *  that's a lot of overhead since TokenManager has linear overhead to
       *  reserve a bit.  To avoid that overhead, we'll just save the ID, but
       *  there's a mess if we ever want more than 64 retrying threads.
       */
      void beginRetry(RetryHandle* handle)
      {
          handle->reset_sem();
      }

      /*** clean up without yielding the CPU */
      void cancelRetry(RetryHandle* handle)
      {
          // unset all read bits and clear the read bit list
          handle->reset();

          // eventually need to return the ID here
      }

      /*** Yield the CPU, then clean up */
      void endRetry(RetryHandle* handle)
      {
          // Wait on semaphore.
          handle->wait();

          // now unset mark bits
          cancelRetry(handle);
      }

      /**
       *  At Descriptor construction time, pass a handle to the manager and get
       *  an ID, which will then be used for the rest of time.
       */
      void init_thread(RetryHandle* handle)
      {
          int id = handles.get_token(handle);
          handle->set_id(id);
      }
  };

  /**
   * typedef one of these mechanisms into the "RetryMechanism" that RSTM will
   * use, based on compile-time defines from config.h
   */
#if   defined(STM_RETRY_SLEEP)
  typedef SleepRetry RetryMechanism;

#elif defined(STM_RETRY_BLOOM)
  typedef BloomRetry RetryMechanism;

#elif defined(STM_RETRY_VISREAD)
  typedef VisReadRetry<rstm::SharedHandle> RetryMechanism;

#endif
}

#endif // RETRY_HPP__
