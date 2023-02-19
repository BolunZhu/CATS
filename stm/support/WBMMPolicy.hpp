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

#ifndef WBMMPOLICY_HPP__
#define WBMMPOLICY_HPP__

#include "defs.hpp"
#include "atomic_ops.h"
#include "MiniVector.hpp"

namespace stm
{

  /*** WBMMPolicyNoEpoch: variant of WBMMPolicy for GCHeap without epochs */
  class WBMMPolicyNoEpoch
  {
      typedef MiniVector<void*> AddressList;

      /*** List of objects to delete if the current transaction commits */
      AddressList frees;

      /*** List of objects to delete if the current transaction aborts */
      AddressList allocs;

      /*** track if we're in a transaction */
      bool isTx;

    public:
      /*** simple constructor */
      WBMMPolicyNoEpoch() : frees(128), allocs(128), isTx(false) { }

      /*** Wrapper to thread-specific allocator for allocating memory */
      void* txAlloc(size_t const &size)
      {
          void* ptr = mm::txalloc(size);
          if (isTx)
              allocs.insert(ptr);
          return ptr;
      }

      /*** Wrapper to thread-specific allocator for freeing memory */
      void txFree(void* ptr)
      {
          if (isTx)
              frees.insert(ptr);
          else
              mm::txfree(ptr);
      }

      /*** On begin, move to an odd epoch and start logging */
      void onTxBegin() { isTx = true; }

      void onTxAbort()
      {
          AddressList::iterator i;
          for (i = allocs.begin(); i != allocs.end(); i++)
              mm::txfree(*i);
          frees.reset();
          allocs.reset();
          isTx = false;
      }

      /*** On commit, perform frees, clear lists, exit epoch */
      void onTxCommit()
      {
          for (AddressList::iterator i = frees.begin(); i != frees.end(); i++)
              mm::txfree(*i);
          frees.reset();
          allocs.reset();
          isTx = false;
      }
  };

  /*** WBMMPolicyEpoch: a simple variant of RADDMMPolicy, for word-based STM */
  class WBMMPolicyEpoch
  {
      typedef MiniVector<void*> AddressList;

      /*** Node type for a list of timestamped void*s */
      struct limbo_t
      {
          /*** Number of void*s held in a limbo_t */
          static const unsigned long POOL_SIZE = 32;

          /*** Set of void*s */
          void* pool[POOL_SIZE];

          /*** Timestamp when last void* was added */
          unsigned long ts[MAX_THREADS];

          /*** # valid timestamps in ts, or # elements in pool */
          unsigned long  length;

          /*** Next pointer for the limbo list */
          limbo_t*       older;

          /*** The constructor for the limbo_t just zeroes out everything */
          limbo_t() : length(0), older(NULL) { }

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

      /*** location of my timestamp value */
      unsigned long* my_ts;

      /*** As we mark things for deletion, we accumulate them here */
      limbo_t* prelimbo;

      /*** sorted list of timestamped reclaimables */
      limbo_t* limbo;

      /*** List of objects to delete if the current transaction commits */
      AddressList frees;

      /*** List of objects to delete if the current transaction aborts */
      AddressList allocs;

      /**
       *  Schedule a pointer for reclamation.  Reclamation will not happen
       *  until enough time has passed.
       */
      void schedForReclaim(void* ptr)
      {
          // insert /ptr/ into the prelimbo pool and increment the pool size
          prelimbo->pool[prelimbo->length++] = ptr;

          // if prelimbo is full, push it onto limbo list and get new prelimbo
          if (prelimbo->length == prelimbo->POOL_SIZE) {
              // get the current timestamp from the epoch
              prelimbo->length = thread_count;
              for (unsigned long i = 0; i < prelimbo->length; i++)
                  prelimbo->ts[i] = trans_nums[i*16];

              // push prelimbo onto the front of the limbo list:
              prelimbo->older = limbo;
              limbo = prelimbo;

              // check if anything after limbo->head is dominated by ts.  Exit
              // the loop when the list is empty, or when we find something
              // that is strictly dominated.
              // NB: the list is in sorted order by timestamp.
              limbo_t* current = limbo->older;
              limbo_t* prev = limbo;
              while (current != NULL) {
                  if (isStrictlyOlder(limbo->ts, current->ts, current->length))
                      break;
                  prev = current;
                  current = current->older;
              }

              // If current != NULL, it is the head of a list of reclaimables
              if (current) {
                  // detach /current/ from the list
                  prev->older = NULL;

                  // free all blocks in each node's pool and free the node
                  while (current != NULL) {
                      // free blocks in current's pool
                      for (unsigned long i = 0; i < current->POOL_SIZE; i++)
                          mm::txfree(current->pool[i]);

                      // free the node and move on
                      limbo_t* old = current;
                      current = current->older;
                      mm::txfree(old);
                  }
              }
              prelimbo = new limbo_t();
          }
      }

    public:

      /**
       *  Constructing the DeferredReclamationMMPolicy is very easy
       *  Null out the timestamp for a particular thread.  We only call this
       *  at initialization.
       */
      WBMMPolicyEpoch()
          : prelimbo(new limbo_t()), limbo(NULL), frees(128), allocs(128)
      {
          unsigned long id = fai(&thread_count);
          my_ts = &trans_nums[id*16];
          *my_ts = 0;
      }

      /*** Wrapper to thread-specific allocator for allocating memory */
      void* txAlloc(size_t const &size)
      {
          void* ptr = mm::txalloc(size);
          if ((*my_ts)&1)
              allocs.insert(ptr);
          return ptr;
      }

      /*** Wrapper to thread-specific allocator for freeing memory */
      void txFree(void* ptr)
      {
          if ((*my_ts)&1)
              frees.insert(ptr);
          else
              mm::txfree(ptr);
      }

      /*** On begin, move to an odd epoch and start logging */
      void onTxBegin() { *my_ts = 1+*my_ts; }

      /*** On abort, unroll allocs, clear lists, exit epoch */
      void onTxAbort()
      {
          AddressList::iterator i;
          for (i = allocs.begin(); i != allocs.end(); i++)
              schedForReclaim(*i);
          frees.reset();
          allocs.reset();
          *my_ts = 1+*my_ts;
      }

      /*** On commit, perform frees, clear lists, exit epoch */
      void onTxCommit()
      {
          for (AddressList::iterator i = frees.begin(); i != frees.end(); i++)
              schedForReclaim(*i);
          frees.reset();
          allocs.reset();
          *my_ts = 1+*my_ts;
      }
  };

#if defined(STM_ALLOCATOR_GCHEAP_NOEPOCH)
  typedef WBMMPolicyNoEpoch WBMMPolicy;
#else
  typedef WBMMPolicyEpoch WBMMPolicy;
#endif

} // namespace stm

#endif // WBMMPOLICY_HPP__
