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

#ifndef LAZYCM_HPP__
#define LAZYCM_HPP__

#include <string>
#include <iostream>
#include "../support/hrtime.h"
#include "../support/Bloom.hpp"
#include "../support/MiniVector.hpp"
#include "../support/Retry.hpp"
#include "../support/word_based_metadata.hpp"

#include <limits.h>
//by zbl limites.h

namespace stm
{
  /*** Common stuff for priority-based contention managers. */
  class BaseCM
  {
    protected:
      // count consecutive aborts
      unsigned consecutive_aborts;

      unsigned backoff_karma_reducer;

      // for randomized exponential backoff on abort
      bool     backoff_on_abort;
      unsigned seed;
      unsigned BACKOFF_MIN;        // default = 4
      unsigned BACKOFF_MAX;        // default = 16

      static volatile unsigned long prioTxCount;
      static volatile unsigned long retryTxCount;

      static volatile unsigned long inev_token;
      bool         inevitable;

      /*** Inevitability epoch, for those mechanisms that need a tfence. */
      static padded_unsigned_t epoch_size;
      static padded_unsigned_t epoch[128];

      unsigned  epoch_slot;
      unsigned* epoch_buff;

      /*** epoch wait for GRL inevitability */
      void epoch_wait()
      {
          // copy the current epoch
          unsigned num_txns = epoch_size.val;
          for (unsigned i = 0; i < num_txns; i++)
              epoch_buff[i] = epoch[i].val;
          // wait for a dominating epoch
          for (unsigned i = 0; i < num_txns; i++) {
              if ((i != epoch_slot) && ((epoch_buff[i] & 1) == 1)) {
                  while (epoch[i].val == epoch_buff[i])
                      spin64();
              }
          }
      }

      /**
       *  Randomized exponential backoff on abort.
       */
      void abort_backoff()
      {
          // how many bits should we use to pick an amount of time to wait?
          unsigned bits = consecutive_aborts + BACKOFF_MIN - 1;
          if (bits > BACKOFF_MAX)
              bits = BACKOFF_MAX;
          // get a random amount of time to wait, bounded by an exponentially
          // increasing limit
          unsigned long delay = rand_r(&seed);
          delay &= ((1 << bits)-1);

          // wait until at least that many ns have passed
          unsigned long long start = getElapsedTime();
          unsigned long long stop_at = start + delay;

          while (getElapsedTime() < stop_at) { spin64(); }
      }

      void inc_PrioTxCount() { fai(&prioTxCount); }
      void dec_PrioTxCount() { faa(&prioTxCount, -1); }

      void inc_RetryTxCount() { fai(&retryTxCount); }
      void dec_RetryTxCount() { faa(&retryTxCount, -1); }

      static volatile bool post_on_retry;

      void abort_common()
      {
          if (backoff_on_abort)
              abort_backoff();
          else
              yield_cpu();
          consecutive_aborts++;
      }

      void commit_common()
      {
          consecutive_aborts = 0;
          if (inevitable) {
              inevitable = false;
              inev_token = 0;
          }
      }

      void retry_common()
      {
          // if I call retry, then when I restart I should not remember
          // previous aborts
          consecutive_aborts = 0;
      }


    public:

      // distinguish between being non-inevitable, inevitable with
      // instrumentation, and inevitable with GRL (i.e. totally isolated)
      enum InevStates { REGULAR, INEVITABLE, ISOLATED };

      BaseCM(std::string POLICY, bool initialize_epoch = true)
          : consecutive_aborts(0), backoff_karma_reducer(16),
            backoff_on_abort(false),
            seed((unsigned long)&consecutive_aborts),
            BACKOFF_MIN(4), BACKOFF_MAX(16)
      {
          if (POLICY == "Patient") {
              backoff_on_abort = false;
          }
          else if (POLICY == "PatientR") {
              backoff_on_abort = true; // default is 4-16
          }
          else if (POLICY.find_first_of('-') < POLICY.size()) {
              // format is C-{K}-{MIN}-{MAX}
              //
              // that is, the first two chars are C- for custom
              // then comes the karma reducer
              // then comes backoff_min
              // then comes backoff_max
              // if min==max==0, then no backoff
              std::string str = POLICY;
              int pos1 = str.find_first_of('-', 0);
              int pos2 = str.find_first_of('-', pos1+1);
              int pos3 = str.find_first_of('-', pos2+1);
              int pos4 = str.size();
              int p1 = atoi(str.substr(pos1+1, pos2-pos1-1).c_str());
              int p2 = atoi(str.substr(pos2+1, pos3-pos2-1).c_str());
              int p3 = atoi(str.substr(pos3+1, pos4-pos3-1).c_str());
              std::string p = str.substr(0, pos1);
              if (p != "C") {
                  std::cerr << "Invalid CM parameter " << POLICY << std::endl;
                  assert(false);
              }
              backoff_karma_reducer = p1;
              if (p2 == 0 && p3 == 0) {
                  backoff_on_abort = false;
              }
              else {
                  backoff_on_abort = true;
                  BACKOFF_MIN = p2;
                  BACKOFF_MAX = p3;
              }
          }
          else
              std::cerr << "Unsupported CM option " << POLICY
                        << "... defaulting to Patient" << std::endl;

          // grl inevitability configuration... only use it when this ctor is
          // called directly, not for child classes that override inevitability
          if (!initialize_epoch)
              return;
          unsigned u = fai(&epoch_size.val);
          epoch_slot = u;
          epoch_buff = (unsigned*)malloc(128 * sizeof(unsigned));
          memset((void*)epoch_buff, 0, 128 * sizeof(unsigned));
      }

      int prio() { return 0; }

      bool try_inevitable()
      {
          // multiple calls by an inevitable transaction have no overhead
          if (inevitable)
              return true;

          // GRL: the inev token is a counter
          unsigned old = inev_token;
          if ((old & 1) == 1)
              return false; // token is held
          if (!bool_cas(&inev_token, old, old+1))
              return false; // missed chance to get the token
          inevitable = true;
          epoch_wait(); // wait for all active transactions to finish
          return true;
      }

      // for compatibility when priority is not supported
      void getPrio(int request) { }
      void beforeOpen(void* addr) { }
      template <class T>
      bool preCommit(T& wset) { return true; }

      /**
       *  Simple Event Methods
       */
      void onBegin()
      {
          // increment my fence, then block if someone is inevitable
          while (true) {
              // open my current epoch
              epoch[epoch_slot].val++;
              WBR; // write to epoch before reading inev_txn_count
              unsigned old = inev_token;
              // if someone is inevitable, then they might be waiting on me,
              // so get out of their way
              if ((old & 1) == 0)
                  break;
              epoch[epoch_slot].val--;
              spin128(); // wait a little while
          }
      }

      void onCommit()
      {
          commit_common();
          epoch[epoch_slot].val++;
      }

      void onAbort()
      {
          abort_common();
          epoch[epoch_slot].val++;
      }

      InevStates get_inev_state() { return (inevitable) ? ISOLATED : REGULAR; }

      /**
       * sleep-based retry.  we don't have support for priority or visible
       * reads, so we don't actually do anything here
       */
      template <class T>
      void preRetry(T& rset) { retry_common(); }

      /***  Nobody is actually waiting on notification, so this is a nop */
      void haltRetry() { }

      /*** call this after validating to finish a retry */
      void postRetry()
      {
          // sleep
          AbstractSchedulerInterface::sleep(50);
      }

      // this is for waking retriers.  we don't do anything, because they wake
      // themselves
      template <class T>
      void postCommit(T& lockset) { }
  };

  /**
   * A contention manager that uses bloom filters to provide priority and
   * fairness
   */
  class BloomCM : public BaseCM
  {
      typedef Bloom<1024, 1> filter_t;

      /*** for creating a linked list of transactions that have priority */
      struct BloomList {
          volatile filter_t   filter;
          volatile int        priority;       // 0 means "ignore this filter"
          AbstractSchedulerInterface* semaphore;
          BloomList* next;
          BloomList()
              : filter(), priority(0),
                semaphore(new AbstractSchedulerInterface()), next(NULL)
          { }
      };

      /**
       * This lets us quickly replace a pointer with an unsigned long for
       * CASing volatile pointers
       */
      union ptr_unsigned_t
      {
          volatile BloomList*    ptr;
          volatile unsigned long ul;
      };

      // class-wide stuff: list of high priority transactions' read sets, and
      // number of high priority transactions
      static ptr_unsigned_t         prioReadSets;

      // stuff local to this particular instance
      BloomList*   rSet;                  // pointer to my rset filter
      volatile int priority;              // my priority
      filter_t*    wSet;                  // my wset
      bool         wsetEmpty;             // is my wset empty?

    public:
      BloomCM(std::string POLICY) : BaseCM(POLICY, false), priority(0),
                                    wsetEmpty(true)
      {
          // get a read filter and put it in the list, but don't bump
          // prioTxCount.
          rSet = new BloomList();
          ptr_unsigned_t me;
          me.ptr = rSet;
          while (true) {
              ptr_unsigned_t head;
              head.ul = prioReadSets.ul;
              rSet->next = const_cast<BloomList*>(head.ptr);
              if (bool_cas(&prioReadSets.ul, head.ul, me.ul))
                  break;
          }

          // get a write filter for doing checks
          wSet = new filter_t();
      }

      int prio() { return priority; }

      void onBegin()
      {
          // grrr... no virtual dispatch == must do this in every child class
          long prio_bump = (backoff_karma_reducer) ? consecutive_aborts / backoff_karma_reducer : 0;
          if (prio_bump)
              getPrio(prio_bump);
      }

      // try to become inevitable
      bool try_inevitable()
      {
          if (inevitable)
              return true;
          if (inev_token)
              return false;
          if (bool_cas(&inev_token, 0, 1)) {
              inevitable = true;
              getPrio(INT_MAX);
          }
          return inevitable;
      }

      InevStates get_inev_state() { return (inevitable) ? INEVITABLE : REGULAR; }

      /*** not sure if we need this on writes or not... */
      void beforeOpen(void* addr)
      {
          if (priority < 1)
              return;
          // add bit to filter, then wbr
          rSet->filter.insert((unsigned)addr);
          // experimental support
#ifdef SKIP_WBR
          if (inevitable)
              WBR;
#else
          WBR;
#endif
          // ok, we've got a prio read on this
      }

      void onAbort()
      {
          abort_common();

          // If I had to construct a wset, clear it here
          if (!wsetEmpty) {
              wsetEmpty = true;
              wSet->reset();
          }

          // If I had priority or was a retryer, release it
          if (priority > 0)
              dec_PrioTxCount();
          else if (priority == -1)
              dec_RetryTxCount();
          if (priority != 0) {
              // unmark my filter and clear it
              rSet->priority = 0;
              priority = 0;
              rSet->filter.reset();
          }
      }

      /**
       * elevate this txn's priority.  note: if you call this mid-stream, you
       * may be disappointed...
       */
      void getPrio(int orig_request)
      {
          // can't go above INT_MAX
          long prio_bump =
              (orig_request == INT_MAX) ? 0 :
              (backoff_karma_reducer) ? consecutive_aborts / backoff_karma_reducer : 0;
          int request = orig_request + prio_bump;

          if (request <= 0)
              return;

          if (request == INT_MAX)
              assert(inevitable);
          priority = request;
          rSet->priority = priority;
          inc_PrioTxCount();
      }

      /*** call this, then validate, then call postRetry */
      template <class T>
      void preRetry(T& rset)
      {
          retry_common();

          // This is really unfortunate.  The filter holds addresses that we
          // acquired.  But now we have a list of orecs and we're waiting on
          // one of them to get locked.  We can't reuse the filter because our
          // rset doesn't give addresses (and logging them on the niagara would
          // cost a lot).  So we have to clear the filter, then re-fill it with
          // orecs that we're waiting on
          if (priority) {
              dec_PrioTxCount();
              rSet->priority = 0;
              priority = 0;
              rSet->filter.reset();
          }

          for (typename T::iterator i = rset.begin(); i != rset.end(); ++i) {
              rSet->filter.insert((unsigned)(void*)*i);
          }
          rSet->semaphore->reset_sem();
          rSet->priority = -1;
          priority = -1;
          inc_RetryTxCount(); // don't need wbr on prio because we have this
                              // CAS
      }

      /**
       *  There's a funny race here.  If there is nobody left to wake people, I
       *  need to keep them from sleeping.  The way we do it is to first ensure
       *  that nobody else will post, and then to wake anyone who is sleeping.
       *  Both steps are necessary, because someone might be about to clear
       *  their filter, in which case they miss my post, and someone might
       *  already be asleep.
       */
      void haltRetry()
      {
          post_on_retry = false;

          volatile BloomList* curr = prioReadSets.ptr;
          while (curr) {
              curr->semaphore->post();
              curr = curr->next;
          }
      }

      /*** call this after validating to finish a retry */
      void postRetry()
      {
          // sleep
          if (post_on_retry)
              rSet->semaphore->wait();

          // unset filter
          dec_RetryTxCount();
          rSet->priority = 0;
          priority = 0;
          rSet->filter.reset();
      }

      // this is for waking retriers.  Note that since retriers use their rset
      // orecs, we use the lockset here.
      template <class T>
      void postCommit(T& lockset)
      {
          // if no retriers, caller can exit immediately
          if (retryTxCount == 0)
              return;

          // \exist retriers.  make a filter of this txn's writes
          //
          // again, unfortunate need to clear wSet since it holds addresses,
          // not orec addresses
          if (wsetEmpty == false) {
              wSet->reset();
          }
          wsetEmpty = false;
          typename T::iterator i;
          for (i = lockset.begin(); i != lockset.end(); ++i) {
              wSet->insert((unsigned)(void*)*i);
          }

          // intersect my filter with all published filters
          volatile BloomList* curr = prioReadSets.ptr;
          while (curr) {
              // only look at filters with -1 priority
              if (curr->priority == -1) {
                  if (wSet->intersect(const_cast<filter_t*>(&curr->filter))) {
                      curr->semaphore->post();
                  }
              }
              curr = curr->next;
          }
      }

      // call preCommit once you hold all your locks
      template <class T>
      bool preCommit(T& wset)
      {
          // if no prio transactions, caller can commit immediately
          if (prioTxCount == 0)
              return true;

          // \exist prio txns.  Make a filter of this txn's writes
          wsetEmpty = false;
          for (typename T::iterator i = wset.begin(); i != wset.end(); ++i) {
              wSet->insert((unsigned)(void*)i->addr);
          }

          // intersect my filter with all published filters
          volatile BloomList* curr = prioReadSets.ptr;
          while (curr) {
              // only look at filters with nonzero priority > my priority
              int cp = curr->priority;
              if ((cp) && (cp > priority)) {
                  if (wSet->intersect(const_cast<filter_t*>(&(curr->filter))))
                  {
                      return false;
                  }
                  // NB: if the filter changes while we access it, we don't
                  // care.  Since that means you started a new high-prio txn
                  // after I saw your old filter, you had to see that I held
                  // the lock, so you won't abort
              }
              curr = curr->next;
          }
          return true;
      }


      /*** NB: this will change to unify retry() */
      void onCommit()
      {
          commit_common();

          // If I had to construct a wset, clear it here
          if (!wsetEmpty) {
              wsetEmpty = true;
              wSet->reset();
          }

          // If I had priority, release it
          if (priority) {
              dec_PrioTxCount();
              // unmark my filter and clear it
              rSet->priority = 0;
              rSet->filter.reset();
              // reset me
              priority = 0;
          }
      }
  };

  /**
   * A contention manager that uses visible readers to provide priority and
   * fairness
   */
  class VisReadCM : public BaseCM
  {
      static const unsigned num_bits = 32;

      /**
       * a reader record (rrec) holds bits representing up to 128 reader
       * transactions
       */
      struct rrec
      {
          /*** num_bits bits, to represent num_bits readers */
          volatile unsigned long bits[num_bits / (8*sizeof(unsigned long))];

          /*** set a bit */
          void setbit(unsigned slot)
          {
              unsigned bucket = slot / 32;
              unsigned mask = 1<<(slot % 32);
              unsigned long oldval = bits[bucket];
              if (oldval & mask)
                  return;
              while (true) {
                  if (bool_cas(&bits[bucket], oldval, oldval | mask))
                      return;
                  oldval = bits[bucket];
              }
          }

          /*** unset a bit */
          void unsetbit(unsigned slot)
          {
              unsigned bucket = slot / 32;
              unsigned mask = 1<<(slot % 32);
              unsigned unmask = ~mask;
              unsigned long oldval = bits[bucket];
              if (!(oldval & mask))
                  return;
              while (true) {
                  if (bool_cas(&bits[bucket], oldval, oldval & unmask))
                      return;
                  oldval = bits[bucket];
              }
          }
      };

      /*** store the priority associated with each bit */
      static volatile unsigned long priorities[num_bits];

      /*** store the semaphore associated with each bit */
      static AbstractSchedulerInterface semaphores[num_bits];

      /*** the number of rrecs */
      static const int rrec_count = 1048576;

      /*** the set of rrecs */
      static rrec rrecs[rrec_count];

      // stuff local to this particular instance
      MiniVector<unsigned long> myRRecs;   // indices of rrecs I set
      volatile int              priority;  // my priority
      int                       myslot;    // 0-127 my vis reader bit

    public:
      VisReadCM(std::string POLICY)
          : BaseCM(POLICY, false), myRRecs(128), priority(0), myslot(-1)
      {
      }

      int prio() { return priority; }

      void onBegin()
      {
          long prio_bump = (backoff_karma_reducer) ?
              consecutive_aborts / backoff_karma_reducer : 0;
          if (prio_bump)
              getPrio(prio_bump);
      }

      // try to become inevitable
      bool try_inevitable()
      {
          if (inevitable)
              return true;
          if (inev_token)
              return false;
          if (bool_cas(&inev_token, 0, 1)) {
              inevitable = true;
              // not guaranteed a slot, so we might have to do this multiple
              // times
              while (priority != INT_MAX)
                  getPrio(INT_MAX);
          }
          return inevitable;
      }

      InevStates get_inev_state()
      {
          return (inevitable) ? INEVITABLE : REGULAR;
      }

      /*** not sure if we need this on writes or not... */
      void beforeOpen(void* addr)
      {
          if (priority < 1)
              return;
          // get the rrec for this address
          int index = (((unsigned)addr) >> 3) % rrec_count;
          // set the bit
          rrecs[index].setbit(myslot);
          // remember the bit
          myRRecs.insert(index);
      }

      void onAbort()
      {
          abort_common();
          // If I had priority, release it
          if (priority > 0)
              dec_PrioTxCount();
          if (priority == -1)
              dec_RetryTxCount();
          if (priority != 0) {
              // give up my priority, release bits, return slot
              priorities[myslot] = 0;
              priority = 0;
              // unset all my read bits
              MiniVector<unsigned long>::iterator i;
              for (i = myRRecs.begin(); i != myRRecs.end(); ++i) {
                  rrecs[*i].unsetbit(myslot);
              }
              myRRecs.reset();
          }
      }

      /**
       * elevate this txn's priority.  note: if you call this mid-stream, you
       * may be disappointed...
       */
      void getPrio(int orig_request)
      {
          // can't go above INT_MAX
          long prio_bump =
              (orig_request == INT_MAX) ? 0 :
              (backoff_karma_reducer) ? consecutive_aborts / backoff_karma_reducer : 0;
          int request = orig_request + prio_bump;

          if (request <= 0)
              return;

          if (request == INT_MAX)
              assert(inevitable);

          // upgrades are easy :)
          if (priority > 0) {
              priorities[myslot] = request;
              priority = request;
              return;
          }

          // ok, we don't have a slot yet.  Let's see if the last slot we held
          // is available, and if so, claim it:
          if (myslot > -1) {
              if (priorities[myslot] == 0) {
                  if (bool_cas(&priorities[myslot], 0, request)) {
                      priority = request;
                      inc_PrioTxCount();
                      return;
                  }
              }
          }

          // that failed.  try every slot
          for (unsigned s = 0; s < num_bits; s++) {
              if (priorities[s] == 0) {
                  if (bool_cas(&priorities[s], 0, request)) {
                      priority = request;
                      myslot = s;
                      inc_PrioTxCount();
                      return;
                  }
              }
          }
          // we failed.  oh well.  thread keeps old priority.
      }

      /*** call this, then validate, then call postRetry */
      template <class T>
      void preRetry(T& rset)
      {
          retry_common();

          // same problem as in BloomCM.  If we have priority, need to return
          // all bits, then grab new bits
          if (priority) {
              dec_PrioTxCount();
              // unset all my read bits
              MiniVector<unsigned long>::iterator i;
              for (i = myRRecs.begin(); i != myRRecs.end(); ++i) {
                  rrecs[*i].unsetbit(myslot);
              }
              myRRecs.reset();
              // at least we don't have to get a slot
              priority = -1;
              priorities[myslot] = -1;
          }
          else {
              // need to get priority

              bool need_slot = true;
              while (need_slot) {
                  // ok, we don't have a slot yet.  Let's see if the last slot
                  // we held is available, and if so, claim it:
                  if (myslot > -1) {
                      if (priorities[myslot] == 0) {
                          if (bool_cas(&priorities[myslot], 0, -1)) {
                              priority = -1;
                              inc_PrioTxCount();
                              need_slot = false;
                          }
                      }
                  }
                  if (need_slot) {
                      // that failed.  try every slot
                      for (unsigned s = 0; s < num_bits; s++) {
                          if (priorities[s] == 0) {
                              if (bool_cas(&priorities[s], 0, -1)) {
                                  priority = -1;
                                  myslot = s;
                                  inc_PrioTxCount();
                                  need_slot = false;
                                  break;
                              }
                          }
                      }
                  }
              }
          }

          // now mark all reads
          for (typename T::iterator i = rset.begin(); i != rset.end(); ++i) {
              // get the rrec for this address
              int index = (((unsigned)(void*)*i) >> 3) % rrec_count;
              // set the bit
              rrecs[index].setbit(myslot);
              // remember the bit
              myRRecs.insert(index);
          }
          semaphores[myslot].reset_sem();
          inc_RetryTxCount();
      }

      /*** see BloomCM */
      void haltRetry()
      {
          post_on_retry = false;
          for (unsigned i = 0; i < num_bits; i++)
              semaphores[i].post();
      }

      /** call this after validating to finish a retry */
      void postRetry()
      {
          // sleep
          if (post_on_retry)
              semaphores[myslot].wait();

          // release bits and slot
          dec_RetryTxCount();

          // unset all my read bits
          MiniVector<unsigned long>::iterator i;
          for (i = myRRecs.begin(); i != myRRecs.end(); ++i) {
              rrecs[*i].unsetbit(myslot);
          }
          myRRecs.reset();
          // at least we don't have to get a slot
          priority = 0;
          priorities[myslot] = 0;
      }

      // this is for waking retriers.  note that since retriers use their rset
      // orecs, we use the lockset here
      template <class T>
      void postCommit(T& lockset)
      {
          // if no retriers, caller can exit immediately
          if (retryTxCount == 0)
              return;

          // \exist retriers.  accumulate read bits for my locks
          unsigned long accumulator[num_bits/(8*sizeof(unsigned long))] = {0};
          typename T::iterator i;
          for (i = lockset.begin(); i != lockset.end(); ++i) {
              int index = (((unsigned)(void*)*i) >> 3) % rrec_count;
              for (unsigned n = 0; n < (num_bits / (8*sizeof(unsigned long))); n++)
                  accumulator[n] |= rrecs[index].bits[n];

          }

          // check the accumulator for bits that represent higher-priority
          // transactions
          for (unsigned slot = 0; slot < num_bits; slot++) {
              unsigned bucket = slot / 32;
              unsigned mask = 1<<(slot % 32);
              if (accumulator[bucket] & mask) {
                  if ((long)priorities[slot] == -1)
                      semaphores[slot].post();
              }
          }
      }


      // call preCommit once you hold all your locks
      template <class T>
      bool preCommit(T& wset)
      {
          // if no prio transactions, caller can commit immediately
          if (prioTxCount == 0)
              return true;

          // \exist prio txns.  accumulate read bits covering addresses in my
          // write set
          unsigned long accumulator[num_bits/(8*sizeof(unsigned long))] = {0};
          for (typename T::iterator i = wset.begin(); i != wset.end(); ++i) {
              int index = (((unsigned)i->addr) >> 3) % rrec_count;
              for (unsigned n = 0; n < (num_bits / (8*sizeof(unsigned long))); n++)
                  accumulator[n] |= rrecs[index].bits[n];
          }

          // check the accumulator for bits that represent higher-priority
          // transactions
          for (unsigned slot = 0; slot < num_bits; slot++) {
              unsigned bucket = slot / 32;
              unsigned mask = 1<<(slot % 32);
              if (accumulator[bucket] & mask) {
                  if ((long)priorities[slot] > priority)
                      return false;
              }
          }
          return true;
      }

      void onCommit()
      {
          commit_common();

          // If I had priority, release it
          if (priority) {
              // give up my priority
              priorities[myslot] = 0;

              // decrease prio count
              dec_PrioTxCount();

              // unset all my read bits
              MiniVector<unsigned long>::iterator i;
              for (i = myRRecs.begin(); i != myRRecs.end(); ++i) {
                  rrecs[*i].unsetbit(myslot);
              }

              // clear metadata, reset priority
              priority = 0;
              myRRecs.reset();
          }
      }
  };

} // stm

#endif // LAZYCM_HPP__
