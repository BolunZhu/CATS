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

#ifndef MMPOLICY_HPP__
#define MMPOLICY_HPP__

#include "defs.hpp"
#include "MiniVector.hpp"
#include "atomic_ops.h"

namespace stm
{
  /**
   *  RADDMMPolicy: a policy for handling Revocable Allocation and Deferred
   *  Destruction of objects.  The class seems a bit complex, but it makes
   *  sense to keep it all together for now and implement other policies as
   *  needed.
   *
   *  In this policy, malloc and free are monitored, and an event method at
   *  the end of a transaction commits or rolls back mm ops accordingly.
   *  Should a free() commit, the corresponding object's destructor is
   *  deferred until a safe point (formerly an 'Epoch'), a point at which it
   *  is known that no running transaction will still hold a reference to the
   *  deleted object.
   *
   *  It happens to be the case that the Epoch mechanism also provides a
   *  solution to privatization, so we expose a little bit of the internals
   *  of the MMPolicy to enable its dual use for privatization, too.
   *
   *  For the time being, there is only one MMPolicy, but that may change
   *  eventually.
   */
  class RADDMMPolicy
  {
      /**
       * Node type for a linked list of objects that are logically deleted,
       * but who cannot be safely destroyed yet.  List nodes hold a set of
       * objects, and a timestamp indicating when destruction will be safe.
       */
      struct limbo_t
      {
          /*** Number of objects in a list node */
          static const unsigned long POOL_SIZE = 32;

          /*** Set of deleted objects */
          void* pool[POOL_SIZE];

          /*** Timestamp indicating when destruction is safe */
          unsigned long* ts;

          /**
           *  Size of the timestamp array if the pool is full, or # elements
           *  in the pool if it is not full and timestamp is not allocated.
           */
          unsigned long  length;

          /*** Next pointer for the limbo list */
          limbo_t*       older;

          /*** The constructor for the limbo_t just zeroes out everything */
          limbo_t() : ts(NULL), length(0), older(NULL) { }

          /*** so we don't have to cast when we allocate these */
          void* operator new(size_t size)  { return mm::txalloc(size); }
      };

      static const unsigned long CACHELINE_SIZE = 16; // words, not bytes

      /*** store every thread's counter */
      static unsigned long trans_nums[MAX_THREADS * CACHELINE_SIZE];

      /*** total thread count*/
      static volatile unsigned long thread_count;

      /*** figure out if one timestamp is strictly dominated by another */
      bool isStrictlyOlder(unsigned long* newer, unsigned long* older,
                           unsigned long old_len)
      {
          for (unsigned long i = 0; i < old_len; i++)
              if ((newer[i] <= older[i]) && (newer[i] % 2 == 1))
                  return false;
          return true;
      }

      /*** copy a timestamp to an un-padded buffer */
      void copy_timestamp(unsigned long* ts_ptr, unsigned long length)
      {
          for (unsigned long i = 0; i < length; i++)
              ts_ptr[i] = trans_nums[i*16];
      }

      /*** Cache of the tx id */
      unsigned long id;

      /**
       *  use this flag to log allocations.  If we're in a transaction,
       *  then anything we allocate should be deleted on abort.  The flag
       *  ensures that we don't have logging overhead for allocations
       *  outside of a transaction.
       */
      bool should_log;

      /*** As we delete things, we accumulate them here */
      limbo_t* prelimbo;

      /*** once prelimbo is full, it moves here */
      limbo_t* limbo;

    public:

      typedef MiniVector<void*> DeleteList;

      /**
       *  List of objects to delete if the current transaction commits.
       *  We use this to schedule objects for deletion from within a
       *  transaction.
       */
      DeleteList deleteOnCommit;

      /**
       *  List of objects to delete if the current transaction aborts.
       *  These are the things we allocated in a transaction that failed.
       */
      DeleteList deleteOnAbort;

    private:
      /**
       *  When the prelimbo list is full, transfer it to the head of the
       *  limbo list, and then reclaim any limbo list entries that are
       *  strictly dominated by the timestamp we gave to the list head.
       */
      void transfer();

    public:

      /**
       *  Constructing the DeferredReclamationMMPolicy is very easy
       *  Null out the timestamp for a particular thread.  We only call this
       *  at initialization.
       */
      RADDMMPolicy()
          : should_log(false), limbo(NULL),
            deleteOnCommit(128), deleteOnAbort(128)
      {
          id = fai(&thread_count);
          prelimbo = new limbo_t();
          trans_nums[id*16] = 0;
      }

      /*** Wrapper to thread-specific allocator for allocating memory */
      void* txAlloc(size_t const &size)
      {
          void* ptr = mm::txalloc(size);
          if (should_log)
              deleteOnAbort.insert(ptr);
          return ptr;
      }

      /*** Wrapper to thread-specific allocator for freeing memory */
      void txFree(void* ptr) { mm::txfree(ptr); }

      /**
       *  To delete an object, use 'add' to add it to the reclaimer's
       *  responsibility.  The reclaimer will figure out when it's safe to
       *  call the object's destructor and operator delete().  This method
       *  simply 'adds' the element to the prelimbo list and then, depending
       *  on the state of the prelimbo, may transfer the prelimbo to the head
       *  of the limbo list.
       */
      void add(void* ptr);

      /*** Event method on beginning of a transaction */
      void onTxBegin()
      {
          // update the gc epoch
          trans_nums[id*16]++;

          // set up alloc logging
          should_log = true;
      }

      /**
       *  Event method on any tx validation call
       *
       *  Right before we do a validation inside of a transaction, there is a
       *  guarantee that if our validation succeeds, then we aren't looking
       *  at anything that has been deleted correctly.  Consequently, we can
       *  safely increment our timestamp, so that long-running transactions
       *  <i>that are still valid and that are not preempted</i> do not
       *  impede the collection activities of other threads.
       *
       *  NB: We need to increment by 2, so that we don't give the mistaken
       *  impression that we are out of a transaction.
       */
      void onValidate()
      {
          // update our epoch.  Either we are going to abort, or else we are
          // not looking at anything that other active threads have deleted
          //
          // NB: if we want privatization with a heavy fence, this cannot be
          // turned on.
#ifndef STM_PRIV_TFENCE
          trans_nums[id*16] += 2;
#endif
      }

      /**
       *  Event method on end of a transaction
       *  Update the gc thread list to note that the current thread is
       *  leaving a transaction
       *  Use the state of the transaction to pick one of the mm logs and
       *  delete everything on it.
       */
      void onTxEnd(unsigned long state)
      {
          if (state == COMMITTED) {
              for (DeleteList::iterator i = deleteOnCommit.begin();
                   i != deleteOnCommit.end(); i++)
                  add(*i);
          }
          else {
              for (DeleteList::iterator i = deleteOnAbort.begin();
                   i != deleteOnAbort.end(); i++)
                  add(*i);
          }
          should_log = false;
          deleteOnCommit.reset();
          deleteOnAbort.reset();

          // update state in GC
          trans_nums[id*16]++;
      }

      /**
       * Block until every transaction has either committed, aborted, or
       * validated.
       */
      void waitForDominatingEpoch();

  };
} // stm

inline void stm::RADDMMPolicy::add(void* ptr)
{
    // insert /ptr/ into the pool and increment the pool size
    prelimbo->pool[prelimbo->length] = ptr;
    prelimbo->length++;

    // check if we need to transfer this prelimbo node onto the limbo list
    if (prelimbo->length == prelimbo->POOL_SIZE) {
        // if so, call transfer() and then make a new prelimbo node.
        transfer();
        prelimbo = new limbo_t();
    }
}

inline void stm::RADDMMPolicy::transfer()
{
    // we're going to move prelimbo to the head of the limbo list, and then
    // clean up anything on the limbo list that has become dominated

    // first get memory and create an empty timestamp
    prelimbo->length = thread_count;
    prelimbo->ts = (unsigned long*)
        mm::txalloc(prelimbo->length * sizeof(unsigned long));
    assert(prelimbo->ts);

    // now get the current timestamp from the epoch.
    copy_timestamp(prelimbo->ts, prelimbo->length);

    // push prelimbo onto the front of the limbo list:
    prelimbo->older = limbo;
    limbo = prelimbo;

    // loop through everything after limbo->head, comparing the timestamp to
    // the head's timestamp.  Exit the loop when the list is empty, or when we
    // find something that is strictly dominated.  NB: the list is in sorted
    // order by timestamp.
    limbo_t* current = limbo->older;
    limbo_t* prev = limbo;
    while (current != NULL) {
        if (isStrictlyOlder(limbo->ts, current->ts, current->length))
            break;
        else {
            prev = current;
            current = current->older;
        }
    }

    // If current != NULL, then it is the head of the kill list
    if (current) {
        // detach /current/ from the list
        prev->older = NULL;

        // for each node in the list headed by current, delete all blocks in
        // the node's pool, delete the node's timestamp, and delete the node
        while (current != NULL) {
            // free blocks in current's pool
            for (unsigned long i = 0; i < current->POOL_SIZE; i++) {
                // NB: can't call delete() because these might not be objects
                // if we used txalloc to get memory directly
                mm::txfree(current->pool[i]);

            }

            // free the timestamp
            mm::txfree(current->ts);

            // free the node and move on
            limbo_t* old = current;
            current = current->older;
            mm::txfree(old);
        }
    }
}

inline void stm::RADDMMPolicy::waitForDominatingEpoch()
{
    // However many threads exist RIGHT NOW is the number of threads to wait
    // on.  Future arrivals don't matter.
    unsigned long numThreads = thread_count;

    // get memory that is big enough to hold a timestamp for each active thread
    unsigned long* ts =
        (unsigned long*)stm::mm::txalloc(numThreads * sizeof(unsigned long));

    // make sure the alloc worked
    assert(ts);

    // copy the global epoch into ts
    copy_timestamp(ts, numThreads);

    // now iterate through ts, and spin on any element that is odd and still
    // matches the global epoch
    unsigned long currentThread = 0;
    while (currentThread < numThreads) {
        // if current entry in ts is odd we have to spin until it increments
        if ((ts[currentThread] & 1) == 1) {
            // get the global value
            unsigned long currEntry = trans_nums[currentThread*16];
            if (currEntry == ts[currentThread]) {
                // spin a bit, then retry this element
                for (int i = 0; i < 128; i++)
                    nop();
                continue;
            }
        }
        // ok, ready to move to next element
        currentThread++;
    }

    // fence is complete.  Free the memory we allocated before for the
    // timestamp.
    stm::mm::txfree(ts);
}

#endif // MMPOLICY_HPP__
