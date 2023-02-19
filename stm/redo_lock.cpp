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

#include "redo_lock.hpp"
#include <iostream>
using std::cout;
using std::endl;

/*** provide backing for the global inevitability fields */
stm::InevPolicy::Global stm::InevPolicy::globals;

/** if ThreadLocalPointer has a static, make sure it is backed in a .o */
#if defined(LOCAL_POINTER_ANNOTATION)
template <> LOCAL_POINTER_ANNOTATION redo_lock::Descriptor*
stm::ThreadLocalPointer<redo_lock::Descriptor>::thr_local_key = NULL;
#endif


/** Hack to ensure that the ValidationPolicy statics are backed */
#ifdef STM_COMMIT_COUNTER
volatile unsigned long
stm::GlobalCommitCounterValidationPolicy::global_counter = 0;
#endif

// These are for MMPolicy's epoch
unsigned long
stm::RADDMMPolicy::trans_nums[MAX_THREADS * CACHELINE_SIZE] = {0};
volatile unsigned long stm::RADDMMPolicy::thread_count = 0;

namespace redo_lock
{
stm::ThreadLocalPointer<Descriptor> currentDescriptor;

#ifdef STM_PRIV_NONBLOCKING
/** Provide backing for the global privatization clock, if we're using
    nonblocking privatization. */
volatile unsigned long privatizer_clock = 0;
#endif

/**
 *  Opens /this/ for reading.  If /this/ is NULL, returns NULL. If a tx opens
 *  the same Shared<T> twice, the pointers retured will be the same.
 *
 *  @param    tx  The current transaction descriptor.
 *
 *  @returns A read-only version of the Object that is being
 *           shared. The read-only-ness is enforced by the API at this
 *           point.
 */
Object* Descriptor::openReadOnly(Object* obj, Validator& v, size_t objsize)
{
  // make sure all parameters meet our expectations
  if (!obj)
    return NULL;

  // set the validator to a null version number; change only if we haven't RW'd
  // /this/
  v.config(0);

  // inevitable tx with GRL can just return the object
  if (inev.isInevitable()) {
    return obj;
  }

  // ensure this tx isn't aborted
  verifySelf();

  // if we have a RW copy of this that we opened lazily, we must return it
  Object* ret = findLazyWrite(obj);
  if (ret) {
    return ret;
  }

  while (true) {
    // read the header of /this/ to a local, opportunistically get data ptr
    metadata_dword_t snap;
    unsigned long ownerState = stm::COMMITTED;
    bool isOwned = false;

    // read the header consistently
    while (true) {
      // read the metadata
      snap = obj->m_metadata;
      // if the version is odd, then /this/ is unowned
      if ((snap.fields.ver.version & 1) == 1) {
        isOwned = false;
        break;
      }

      // if the version is 2, then /this/ is locked
      else if (snap.fields.ver.version == 2) {
        cm.onContention();
        verifySelf();
        continue;
      }

      // otherwise we need to get the owner's state and then verify consistency
      // of the snap
      else {
        isOwned = true;
        ownerState = snap.fields.ver.owner->tx_state;
        metadata_dword_t snap2(obj->m_metadata);
        if (snap2.dword == snap.dword)
          break;
      }
    }

    // if /this/ is owned, we might have to call CM or commit a redo log
    if (isOwned) {
      // if owner is ACTIVE, either I own it (reopen) or I must call CM
      if (ownerState == stm::ACTIVE) {
        if (snap.fields.ver.owner == this) {
          cm.onReOpen();
          return snap.fields.redoLog;
        }

        // we aren't the owner; if we can't abort the owner we restart (with a
        // contention manager call).  If we can abort the owner we abort him,
        // try to cleanup, and then restart
        stm::ConflictResolutions r=cm.onRAW(snap.fields.ver.owner->cm.getCM());
        if (r == stm::AbortSelf)
          abort();
        if ((r == stm::Wait) || (!bool_cas(&(snap.fields.ver.owner->tx_state),
                                           stm::ACTIVE, stm::ABORTED))) {
          cm.onContention();
          verifySelf();
          continue;
        }

        // now ensure that we fallthrough to the CleanOnAbort code
        ownerState = stm::ABORTED;
      }

      // if current owner is aborted use CleanOnAbort
      if (ownerState == stm::ABORTED) {
        clearClone(obj, snap.fields.ver.owner, snap.fields.redoLog);
      }
      else {
        // we had better be looking at a committed object
        assert(ownerState == stm::COMMITTED);
        redoClone(obj, snap.fields.ver.owner, snap.fields.redoLog, objsize);
      }
      // restart the loop so we can get a clean version number
      continue;
    }

    // the snapshot should have an odd version number
    assert((snap.fields.ver.version & 1) == 1);

    // set up the validator
    v.config(snap.fields.ver.version);

    // can cause duplicates if we don't use addValidateRead()
    if (conflicts.shouldValidate()) {
      if (conflicts.isValidatingInsertSafe()) {
        insertAndVerifyReads(obj, snap.fields.ver.version);
        verifyLazyWrites();
      }
      else {
        reads.insert(ReadLogEntry(obj, snap.fields.ver.version));
        validate();
      }
    }
    else {
      reads.insert(ReadLogEntry(obj, snap.fields.ver.version));
    }

    verifySelf();

    // notify cm and return
    cm.onOpenRead();
    return obj;
  } // end while (true)
}



/**
 *  Open an object in write mode. If /this/ is null, return null, otherwise
 *  return a clone of the object; if called multiple times by a tx on the same
 *  Object, return same pointer each time
 *
 *  @param    tx  The current transaction descriptor.
 *
 *  @returns      A writeable version of the shared Object.
 */
Object* Descriptor::openReadWrite(Object* obj, size_t objsize)
{
  // make sure all parameters meet our expectations
  if (!obj)
    return NULL;

  // inevitable tx with GRL can just return the object
  if (inev.isInevitable()) {
    return obj;
  }

  // ensure this tx isn't aborted
  verifySelf();

  // make sure that our conflict detection strategy knows we've got a write
  conflicts.onRW();

  // if we have an RW copy of this that we opened lazily, return it
  Object* ret = findLazyWrite(obj);
  if (ret) {
    return ret;
  }

  while (true) {
    // read the header of /this/ to a local, opportunistically get data ptr
    metadata_dword_t snap;
    unsigned long ownerState = stm::COMMITTED;
    bool isOwned = false;

    // read the header consistently
    while (true) {
      // read the metadata
      snap = obj->m_metadata;
      // if the version is odd, then /this/ is unowned
      if ((snap.fields.ver.version & 1) == 1) {
        isOwned = false;
        break;
      }

      // if the version is 2, then /this/ is locked
      else if (snap.fields.ver.version == 2) {
        cm.onContention();
        verifySelf();
        continue;
      }

      // otherwise we need to get the owner's state and then verify consistency
      // of the snap
      else {
        isOwned = true;
        ownerState = snap.fields.ver.owner->tx_state;
        metadata_dword_t snap2(obj->m_metadata);
        if (snap2.dword == snap.dword)
          break;
      }
    }

    // if /this/ is owned, we might have to call CM or commit a redo log
    if (isOwned) {
      // if owner is ACTIVE, either I own it (reopen) or I must call CM
      if (ownerState == stm::ACTIVE) {
        if (snap.fields.ver.owner == this) {
          cm.onReOpen();
          return snap.fields.redoLog;
        }
        // we aren't the owner; if we can't abort the owner we restart (with a
        // contention manager call).  If we can abort the owner we abort him,
        // try to cleanup, and then restart
        stm::ConflictResolutions r=cm.onWAW(snap.fields.ver.owner->cm.getCM());
        if (r == stm::AbortSelf)
          abort();
        if ((r == stm::Wait) || (!bool_cas(&(snap.fields.ver.owner->tx_state),
                                           stm::ACTIVE, stm::ABORTED))) {
          cm.onContention();
          verifySelf();
          continue;
        }

        // now ensure that we fallthrough to the CleanOnAbort
        // code
        ownerState = stm::ABORTED;
      }

      // if current owner is aborted use CleanOnAbort
      if (ownerState == stm::ABORTED) {
        clearClone(obj, snap.fields.ver.owner, snap.fields.redoLog);
      }
      else {
        // we had better be looking at a committed object
        assert(ownerState == stm::COMMITTED);
        redoClone(obj, snap.fields.ver.owner, snap.fields.redoLog, objsize);
      }
      // restart loop so we can get a clean version number
      continue;
    }

    // the snapshot should have an odd version number
    assert((snap.fields.ver.version & 1) == 1);

    // clone the object and make me the owner of the new version
    Object* new_version = (Object*)memcpy(mm.txAlloc(objsize), obj, objsize);
    // NB: memcpy copies the vtable pointer, too

    assert(new_version);
    // redoLog of clone is a backpointer to the parent
    new_version->m_metadata.fields.redoLog = obj;
    // version# of the clone is the version# of the parent
    new_version->m_metadata.fields.ver.version = snap.fields.ver.version;

    if (isLazy) {
      // LAZY: just add /this/ to my lazy writeset... don't
      // acquire
      lazyWrites.insert(LazyWriteLogEntry(obj, snap.fields.ver.version,
                                          new_version, objsize));
    }
    else {
      // EAGER: CAS in a new header, retry open_RW on failure
      if (!acquire(obj, snap.fields.ver.version, new_version)) {
        // this isn't entirely correct if we allocated memory
        // for nested objects...
        mm.deleteOnCommit.insert(new_version);
        new_version = NULL;
        cm.onContention();
        verifySelf();
        continue;
      }

      // CAS succeeded: bookkeep the eager write
      eagerWrites.insert(EagerWriteLogEntry(obj, new_version,objsize));
    }

    // The redoLog is already deleteOnAbort; mark it deleteOnCommit
    // too, since it will always be deleted once this tx is
    // finished
    mm.deleteOnCommit.insert(new_version);

    // Validate, notify cm, and return
    if (conflicts.shouldValidate()) {
      validate();
    }

    verifySelf();
    // don't need this call, since we openWrite only when we
    // acquire...
    cm.onOpenWrite();
    return new_version;
  } // end while (true)
}

void Descriptor::open_privatized(Object* obj, size_t objsize) {
  if (obj == NULL)
    return;

#if !defined(STM_PRIV_TFENCE) && !defined(STM_PRIV_LOGIC)
  while (true) {
    // read the header of obj to a local, opportunistically get data ptr
    metadata_dword_t snap;
    unsigned long ownerState(stm::COMMITTED);
    bool isOwned(false);

    // read the header consistently
    while (true) {
      // read the metadata
      snap = obj->m_metadata;
      // if the version is odd, then /this/ is unowned
      if ((snap.fields.ver.version & 1) == 1) {
        isOwned = false;
        break;
      }

      // if the version is 2, then /this/ is locked so spin a bit
      // and retry
      else if (snap.fields.ver.version == 2) {
        for (int i = 0; i < 128; i++)
          nop();
        continue;
      }

      // otherwise we need to get the owner's state and then
      // verify consistency of the snap
      else {
        isOwned = true;
        ownerState = snap.fields.ver.owner->tx_state;
        if (metadata_dword_t(obj->m_metadata).dword == snap.dword)
          break;
      }
    }

    // if /this/ is owned, kill the owner
    if (isOwned) {
      // if owner is ACTIVE, abort him
      if (ownerState == stm::ACTIVE) {
        // if abort fails, restart loop
        if (!bool_cas(&(snap.fields.ver.owner->tx_state),
                      stm::ACTIVE, stm::ABORTED))
        {
          continue;
        }

        // ensure that we fallthrough to the CleanOnAbort code
        ownerState = stm::ABORTED;
      }

      // if current owner is aborted use CleanOnAbort
      if (ownerState == stm::ABORTED) {
        clearClone(obj, snap.fields.ver.owner,
                   snap.fields.redoLog);
      }
      else {
        // we had better be looking at a committed object
        assert(ownerState == stm::COMMITTED);
        redoClone(obj, snap.fields.ver.owner,
                  snap.fields.redoLog, objsize);
      }
      // restart the loop so we can get a clean version number
      continue;
    }

    // the snapshot should have an odd version number
    assert((snap.fields.ver.version & 1) == 1);

    return;
  }
#endif
}
}

void stm::shutdown(unsigned long i) {
  // we're not going to bother with making this nonblocking...
  static volatile unsigned long mtx = 0;
  while (!bool_cas(&mtx, 0, 1)) { }

  cout << "Thread:" << i
       << "; Commits: "  << redo_lock::currentDescriptor->getCommits()
       << "; Aborts: "   << redo_lock::currentDescriptor->getAborts()
       << "; Retrys: "   << redo_lock::currentDescriptor->getRetrys()
       << endl;

  // [todo] we should merge this thread's reclaimer into a global
  // reclaimer to prevent free memory from being logically unreclaimable.
  mtx = 0;
}
