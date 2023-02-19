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

#include <iostream>
using std::cout;
using std::endl;

#include "rstm.hpp"
using namespace rstm;

/*** provide backing for the global inevitability fields */
stm::InevPolicy::Global stm::InevPolicy::globals;

stm::ThreadLocalPointer<Descriptor> rstm::currentDescriptor;

/*** if ThreadLocalPointer has a static, make sure it is backed in a .o */
#if defined(LOCAL_POINTER_ANNOTATION)
template <> LOCAL_POINTER_ANNOTATION Descriptor*
stm::ThreadLocalPointer<Descriptor>::thr_local_key = NULL;
#endif

#ifdef STM_COMMIT_COUNTER
/*** ensure that the ValidationPolicy statics are backed */
volatile unsigned long
stm::GlobalCommitCounterValidationPolicy::global_counter = 0;
#endif

#ifdef STM_PRIV_NONBLOCKING
/***  Provide backing for the privatization counter */
volatile unsigned long rstm::privatizer_clock = 0;
#endif

/**
 *  Perform the 'transactional fence'.  If we're using a Validation fence or
 *  Transactional fence, then globalEpoch.HeavyFence() will do all the work
 *  (its behavior varies based on compile flags).  If we're doing
 *  obstruction-free privatization, then simply increment the global
 *  privatization counter.
 */
void rstm::fence()
{
#ifdef STM_PRIV_NONBLOCKING
  fai(&privatizer_clock);
#else
  currentDescriptor->mm.waitForDominatingEpoch();
#endif
}
void rstm::acquire_fence() { currentDescriptor->mm.waitForDominatingEpoch(); }
void rstm::release_fence() { currentDescriptor->mm.waitForDominatingEpoch(); }

/*** back the token manager for mapping Descriptors to vis reader bits */
stm::TokenManager<Descriptor> rstm::readbits(32);

// These are for MMPolicy's epoch
unsigned long
stm::RADDMMPolicy::trans_nums[MAX_THREADS * CACHELINE_SIZE] = {0};

volatile unsigned long stm::RADDMMPolicy::thread_count = 0;

void rstm::thr_shutdown(unsigned long i)
{
  // we're not going to bother with making this nonblocking...
  static volatile unsigned long mtx = 0;
  while (!bool_cas(&mtx, 0, 1)) { }

  cout << "Thread:" << i
       << "; Commits: "  << currentDescriptor->getCommits()
       << "; Aborts: "   << currentDescriptor->getAborts()
       << "; Retrys: "   << currentDescriptor->getRetrys()
       << endl;

  // we should merge this thread's reclaimer into a global reclaimer to prevent
  // free memory from being logically unreclaimable.
  mtx = 0;
}

void rstm::thr_init(std::string cm_type, std::string validation,
                    bool use_static_cm)
{
  // initialize mm for this thread
  stm::mm::initialize();

  // create a Descriptor for the new thread and put it in the global table
  currentDescriptor.set(new Descriptor(cm_type, validation, use_static_cm));
}


Object* Descriptor::openReadOnly(SharedHandle* const header) {
  // make sure all parameters meet our expectations
  if (!header)
    return NULL;

  assert_not_deleted(header);

  // inevitable tx with GRL can just return the object
  if (inev.isInevitable())
    return header->m_payload;

  verifySelf();

  // if we have a RW copy of this that we opened lazily, we must return it
  Object* ret = lookupLazyWrite(header);
  if (ret) {
    return ret;
  }

  while (true) {
    // read the header of /this/ to a local, opportunistically get data ptr
    Object* snap;
    Object* newer;
    Object* older = NULL;
    Descriptor* owner = NULL;
    unsigned long ownerState = ~0;
    bool isOwned;

    snap = const_cast<Object*>(header->m_payload);
    newer = get_data_ptr(snap);
    assert(newer);
    assert_not_deleted(newer);
    isOwned = is_owned(snap);
    if (isOwned) {
      older = newer->m_next;
      owner = const_cast<Descriptor*>(newer->m_owner);
      ownerState = owner->tx_state;
    }

    // if /this/ is owned, newer may not be the right ptr to use
    if (isOwned) {
      // if the owner is stm::ACTIVE, either I own it or I must call CM
      if (ownerState == stm::ACTIVE) {
        if (owner == this) {
          // should be CMReOpenWritableAsReadable()
          cm.onReOpen();
          return newer;
        }

        // continue unless we kill the owner; if kill the owner, use older
        // version
        stm::ConflictResolutions r = cm.onRAW(owner->cm.getCM());
        if (r == stm::AbortSelf)
          abort();
        if (r == stm::Wait ||
            !bool_cas(&(owner->tx_state), stm::ACTIVE, stm::ABORTED))
        {
          cm.onContention();
          verifySelf();
          continue;
        }

        // now ensure that we fallthrough to the cleanOnAbort code
        ownerState = stm::ABORTED;
      }

      // if current owner is aborted use cleanOnAbort
      if (ownerState == stm::ABORTED) {
        if (!CleanOnAbort(header, snap, older)) {
          // if cleanup failed; if snap != older there is contention.
          // Otherwise, a like-minded tx did the cleanup for us
          if (header->m_payload != older) {
            cm.onContention();
            verifySelf();
            continue;
          }
        }
        // someone (not necessarily us) cleaned the object to the state we
        // wanted, so from here on out old version is valid
        newer = older;
        assert(newer);
        assert_not_deleted(newer);
      }
      else {
        // we had better be looking at a committed object
        assert(ownerState == stm::COMMITTED);

        if (!CleanOnCommit(header, newer)) {
          // cleanOnCommit failed; if payload != newer there is contention.
          // Otherwise someone did the cleanup for us
          if (header->m_payload != newer) {
            cm.onContention();
            verifySelf();
            continue;
          }
        }
      }
    }

    // try to be a visible reader
    if (isVisible) {
      // install as a vis reader, and bookkeep so we can uninstall later
      if (installVisibleReader(header))
        visibleReads.insert(header);

      // verify that the header of /this/ hasn't changed; if it changed, at
      // least one writer acquired /this/, and we may be incorrect
      if (newer != header->m_payload) {
        abort();
      }

      // validate
      validate();
    }
    else {
      // can cause duplicates if we don't use addValidateInvisRead()
      if (conflicts.shouldValidate()) {
        if (conflicts.isValidatingInsertSafe()) {
          addValidateInvisRead(header, newer);
          verifyLazyWrites();
        }
        else {
          invisibleReads.insert(invis_bookkeep_t(header, newer));
          validate();
        }
      }
      else {
        invisibleReads.insert(invis_bookkeep_t(header, newer));
      }
    }
    verifySelf();

    // notify cm, update read count, and return
    cm.onOpenRead();

    return newer;
  } // end while (true)
}

Object*
Descriptor::openReadWrite(SharedHandle* const header, const size_t objsize)
{
  // make sure all parameters meet our expectations
  if (!header)
    return NULL;

  assert_not_deleted(header);

  // inevitable tx with GRL can just return the object
  if (inev.isInevitable()) {
    return header->m_payload;
  }

  // ensure this tx isn't aborted
  verifySelf();

  // make sure that our conflict detection strategy knows we've got a write
  conflicts.onRW();

  // if we have an RW copy of this that we opened lazily, return it
  Object* ret = lookupLazyWrite(header);
  if (ret)
    return ret;

  while (true) {
    // get consistent view of the object:
    Object* snap;
    Object* newer;
    Object* older = NULL;
    Descriptor* owner = NULL;
    unsigned long ownerState = ~0;
    bool isOwned;


    snap = const_cast<Object*>(header->m_payload);
    newer = get_data_ptr(snap);
    assert(newer);
    assert_not_deleted(newer);
    isOwned = is_owned(snap);
    if (isOwned) {
      older = newer->m_next;
      owner = const_cast<Descriptor*>(newer->m_owner);
      ownerState = owner->tx_state;
    }

    // if /this/ is owned, curr_version may not be right
    if (isOwned) {
      // if the owner is stm::ACTIVE, either I own it or I must call CM
      if (ownerState == stm::ACTIVE) {
        if (owner == this) {
          cm.onReOpen();
          return newer;
        }

        // continue unless we kill the owner; if kill the owner, use older
        // version
        stm::ConflictResolutions r = cm.onWAW(owner->cm.getCM());
        if (r == stm::AbortSelf)
          abort();
        if (r == stm::Wait
            || !bool_cas(&(owner->tx_state), stm::ACTIVE, stm::ABORTED)) {
          cm.onContention();
          verifySelf();
          continue;
        }

        // now ensure that we fallthrough to the cleanOnAbort code
        ownerState = stm::ABORTED;
      }

      // if current owner is aborted use cleanOnAbort if we are lazy, else just
      // plan on using older
      if (ownerState == stm::ABORTED) {
        if (isLazy) {
          // if lazy, we must clean header:
          if (!CleanOnAbort(header, snap, older)) {
            // cleanup failed; if snap != older there is contention.
            // Otherwise, a like-minded tx did the cleanup for us
            if (header->m_payload != older) {
              cm.onContention();
              verifySelf();
              continue;
            }
          }
        }
        // ok, either we are lazy and we cleaned the object, or we're eager and
        // we're going to cas out the old header shortly.  In either case,
        // older is the valid version
        newer = older;
        assert(newer);
        assert_not_deleted(newer);
      }
      else {
        // we had better be looking at a committed object
        assert(ownerState == stm::COMMITTED);
        if (isLazy) {
          // if lazy, we must clean header:
          if (!CleanOnCommit(header, newer)) {
            // cleanOnCommit failed; if payload != newer there is contention.
            // Otherwise someone cleaned up for us
            if (header->m_payload != newer) {
              cm.onContention();
              verifySelf();
              continue;
            }
          }
        }
      }
    }

    // EAGER: continue if we can't abort all visible readers
    bool canAbortAll = true;
    {
      unsigned int index = 0;
      unsigned int bits = header->m_readers;
      while (bits) {
        if (bits & 1) {
          Descriptor* reader = readbits.lookup(index);
          if (reader != this) {
            // if can't abort this reader, return false
            stm::ConflictResolutions r = cm.onWAR(reader->cm.getCM());
            if (r == stm::AbortSelf)
              abort();
            if (r == stm::Wait) {
              canAbortAll = false;
              break;
            }
          }
        }
        bits >>= 1;
        ++index;
      }
    }

    if (!isLazy && !canAbortAll) {
      cm.onContention();
      verifySelf();
      continue;
    }

    // clone the object and make me the owner of the new version NB: the clone
    // actually copies the vtable pointer, too
    Object* new_version = (Object*)memcpy(mm.txAlloc(objsize), newer, objsize);

    assert(new_version);
    // new_version->m_st = header; // NB: happens in the bitcopy
    new_version->m_next = newer;
    new_version->m_owner = this;

    if (isLazy) {
      // LAZY: just add /this/ to my lazy writeset and mark the old version for
      // delete on commit
      lazyWrites.insert(lazy_bookkeep_t(header, newer, new_version));
      mm.deleteOnCommit.insert(newer);
    }
    else {
      // EAGER: CAS in a new header, retry open_RW on failure
      if (!SwapHeader(header, snap, set_lsb(new_version))) {
        mm.deleteOnCommit.insert(new_version);
        // txAlloc in MMPolicy.h arranges to deleteOnAbort everything we
        // allocate, so we only deallocate this failed clone on commit.  It's
        // something of a shame, because no one but us has ever seen this;
        // running it through the reclaimer wastes time.
        new_version = NULL;
        cm.onContention();
        verifySelf();
        continue;
      }

      // CAS succeeded: abort visible readers, add /this/ to my eager writeset,
      // and mark /curr_version/ for deleteOnCommit
      abortVisibleReaders(header);
      mm.deleteOnCommit.insert(newer);
      eagerWrites.insert(eager_bookkeep_t(header, newer, new_version));
    }

    // Validate, notify cm, and return
    if (conflicts.shouldValidate())
      validate();

    verifySelf();
    cm.onOpenWrite();
    return new_version;
  } // end while (true)
}

Object* rstm::open_privatized(SharedHandle* const header) {
  if (!header)
    return NULL;
#if defined(STM_PRIV_TFENCE) || defined(STM_PRIV_LOGIC)
  Object* snap = const_cast<Object*>(header->m_payload);
  Object* newer = get_data_ptr(snap);

  assert(!is_owned(snap));

  return newer;
#else
  while (true) {
    // read the header of /this/ to a local, opportunistically get data ptr
    Object* snap;
    Object* newer;
    Object* older = NULL;
    Descriptor* owner = NULL;
    unsigned long ownerState = stm::COMMITTED;
    bool isOwned = false;

    while (true) {
      // get consistent view of metadata
      snap = const_cast<Object*>(header->m_payload);
      newer = get_data_ptr(snap);
      isOwned = is_owned(snap);
      if (!isOwned)
        break;

      older = newer->m_next;

      owner = const_cast<Descriptor*>(newer->m_owner);
      ownerState = owner->tx_state;

      if (header->m_payload == snap)
        break;
    }

    // if /this/ is owned, newer may not be the right ptr to use
    if (isOwned) {
      // if the owner is stm::ACTIVE, abort him
      if (ownerState == stm::ACTIVE) {
        // if abort fails, restart loop
        if (!bool_cas(&(owner->tx_state), stm::ACTIVE, stm::ABORTED))
          continue;

        // now ensure that we fallthrough to the cleanOnAbort code
        ownerState = stm::ABORTED;
      }

      // if current owner is aborted use cleanOnAbort
      if (ownerState == stm::ABORTED) {
        if (!Descriptor::CleanOnAbort(header, snap, older)) {
          // if cleanup failed; if snap != older there is contention.
          // Otherwise, a like-minded tx did the cleanup for us
          if (header->m_payload != older)
            continue;
        }
        // someone (not necessarily us) cleaned the object to the state we
        // wanted, so from here on out old version is valid
        newer = older;
      }
      else {
        // we had better be looking at a committed object
        assert(ownerState == stm::COMMITTED);

        if (!Descriptor::CleanOnCommit(header, newer))
          // cleanOnCommit failed; if payload != newer there is contention.
          // Otherwise someone did the cleanup for us
          if (header->m_payload != newer)
            continue;
      }
    }

    // we're going to use whatever is in newer from here on out
    assert(newer);
    return newer;
  } // end while (true)
#endif
}

/**
 *  retry via self-abort: the behavior is almost identical to abort, except
 *  that we throw a different exception so we can land in a different catch
 *  block.
 */
__attribute__((flatten))
void rstm::retry() {
  assert(!not_in_transaction());

  // we cannot be inevitable and call this!
  assert(!currentDescriptor->inev.isInevitable());

  currentDescriptor->retry();
}

stm::RetryMechanism rstm::Descriptor::retryImpl;

#if defined(STM_RETRY_SLEEP)
/***  Implementation of retry that uses calls to usleep() */
void rstm::Descriptor::retry() {
  // set state to aborted.  If this fails, we've been aborted, so we shouldn't
  // sleep later
  bool sleep_at_end = bool_cas(&tx_state, stm::ACTIVE, stm::ABORTED);

  // un-acquire headers: note that LAZY won't acquire anything yet, so we can
  // cleanup and just free the list
  cleanupEagerWrites(stm::ABORTED);
  lazyWrites.reset();

  // uninstall visible readers, zero read sets
  cleanupVisReads();
  invisibleReads.reset();

  // commit memory changes and reset memory logging
  mm.onTxEnd(stm::ABORTED);

  // exit inevitability
  inev.onEndTx();

  // sleep only if we didn't take a remote abort
  if (sleep_at_end) {
    ++num_retrys;
    retryImpl.endRetry(retryHandle);
  }
  else {
    ++num_aborts;
    cm.onTxAborted();
  }

  // Unwind
#if defined(STM_ROLLBACK_THROW)
  throw stm::RollBack();
#elif defined(STM_ROLLBACK_SETJMP)
  nesting_depth = 0;
  longjmp(*setjmp_buf, 1);
#endif
}

#elif defined(STM_RETRY_BLOOM)
/*** Bloom-filter based retry */
void rstm::Descriptor::retry() {
  // create the bloom filter
  retryHandle->reset();
  for (LazyWriteLog::iterator i = lazyWrites.begin(),
         e = lazyWrites.end(); i != e; ++i)
    retryHandle->insert(i->shared);

  for (EagerWriteLog::iterator i = eagerWrites.begin(),
         e = eagerWrites.end(); i != e; ++i)
    retryHandle->insert(i->shared);

  for (VisReadLog::iterator i = visibleReads.begin(),
         e = visibleReads.end(); i != e; ++i)
    retryHandle->insert(*i);

  for (InvisReadLog::iterator i = invisibleReads.begin(),
         e = invisibleReads.end(); i != e; ++i)
    retryHandle->insert(i->shared);

  // put the filter into the global list
  retryImpl.beginRetry(retryHandle);

  // validate
  for (LazyWriteLog::iterator i = lazyWrites.begin(),
         e = lazyWrites.end(); i != e && tx_state == stm::ACTIVE; ++i)
    if (!isCurrent(i->shared, i->read_version))
      tx_state = stm::ABORTED;

  for (InvisReadLog::iterator i = invisibleReads.begin(),
         e = invisibleReads.end(); i != e && tx_state == stm::ACTIVE; ++i)
    if (!isCurrent(i->shared, i->read_version))
      tx_state = stm::ABORTED;

  // now we can uninstall self as eager owner / vis reader
  for (EagerWriteLog::iterator i = eagerWrites.begin(),
         e = eagerWrites.end(); i != e; ++i)
    CleanOnAbort(i->shared, i->write_version, i->read_version);

  for (VisReadLog::iterator i = visibleReads.begin(),
         e = visibleReads.end(); i != e; ++i)
    removeVisibleReader(*i);

  // It is important that we do this (1) as a CAS and not a store, (2) AFTER
  // beginRetry(), and (3) after we've uninstalled ourselves on our eager/vis
  // sets
  bool sleep_at_end = bool_cas(&tx_state, stm::ACTIVE, stm::ABORTED);

  // now we can reset all lists
  lazyWrites.reset();
  eagerWrites.reset();
  visibleReads.reset();
  invisibleReads.reset();

  // we're on the brink of sleeping... exit the inev epoch so that a GRL
  // transaction can start inevitably
  inev.onEndTx();

  // we can also exit our MM epoch now, since we don't have any references
  // hanging around to shared data
  mm.onTxEnd(stm::ABORTED);

  if (!sleep_at_end) {
    retryImpl.cancelRetry(retryHandle);
    cm.onTxAborted();
    ++num_aborts;
  }
  else {
    // validation was OK.
    retryImpl.endRetry(retryHandle);
    ++num_retrys;
  }

  // Unwind
#if defined(STM_ROLLBACK_THROW)
  throw stm::RollBack();
#else
  nesting_depth = 0;
  longjmp(*setjmp_buf, 1);
#endif
}

#elif defined(STM_RETRY_VISREAD)
/*** retry via retry bits set in the object header of every object */
void rstm::Descriptor::retry() {
  // must make this call early, because once we call insert() even once,
  // someone could wake us
  retryImpl.beginRetry(retryHandle);

  // We are simultaneously going to roll back and populate the RetryHandle's
  // notion of read/write sets.

  // eager writes: install as retry on each, then clean on abort each
  for (EagerWriteLog::iterator i = eagerWrites.begin(),
         e = eagerWrites.end(); i != e; ++i) {
    retryHandle->insert(i->shared);
    CleanOnAbort(i->shared, i->write_version, i->read_version);
  }

  // visible reads: install as retry on each, then unset vis read bit
  for (VisReadLog::iterator i = visibleReads.begin(),
         e = visibleReads.end(); i != e; ++i) {
    retryHandle->insert(*i);
    removeVisibleReader(*i);
  }

  // lazy writes and invis reads: install retry, then test if current
  for (LazyWriteLog::iterator i = lazyWrites.begin(),
         e = lazyWrites.end(); i != e && tx_state == stm::ACTIVE; ++i) {
    retryHandle->insert(i->shared);
    if (!isCurrent(i->shared, i->read_version))
      tx_state = stm::ABORTED;
  }

  for (InvisReadLog::iterator i = invisibleReads.begin(),
         e = invisibleReads.end(); i != e && tx_state == stm::ACTIVE; ++i) {
    retryHandle->insert(i->shared);
    if (!isCurrent(i->shared, i->read_version))
      tx_state = stm::ABORTED;
  }

  // It is important that we do this (1) as a CAS and not a store, (2) AFTER
  // beginRetry(), and (3) after we've uninstalled ourselves on our eager/vis
  // sets
  bool sleep_at_end = bool_cas(&tx_state, stm::ACTIVE, stm::ABORTED);

  // now we can reset all lists
  lazyWrites.reset();
  eagerWrites.reset();
  visibleReads.reset();
  invisibleReads.reset();

  // we're on the brink of sleeping... exit the inev epoch so that a GRL
  // transaction can start inevitably
  inev.onEndTx();

  if (!sleep_at_end) {
    retryImpl.cancelRetry(retryHandle);
    cm.onTxAborted();
    ++num_aborts;
  }
  else {
    // validation was OK.
    retryImpl.endRetry(retryHandle);
    ++num_retrys;
  }

  mm.onTxEnd(stm::ABORTED);

  // Unwind
#if defined(STM_ROLLBACK_THROW)
  throw stm::RollBack();
#else
  nesting_depth = 0;
  longjmp(*setjmp_buf, 1);
#endif
}
#else
assert(false); // #error "No STM_RETRY_ option specified"
#endif // STM_RETRY_*
