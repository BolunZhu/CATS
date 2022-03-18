/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/**
 *  DelayByteEager Implementation
 *
 *    This is a good-faith implementation of the TLRW algorithm by Dice and
 *    Shavit, from SPAA 2010.  We use bytelocks, eager acquire, and in-place
 *    update, with timeout for deadlock avoidance.
 */

#include "../profiling.hpp"
#include "algs.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::ByteLockList;
using stm::bytelock_t;
using stm::get_bytelock;
using stm::UndoLogEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct DelayByteEager
  {
      static TM_FASTCALL bool begin(TxThread*);
      static TM_FASTCALL void* read_ro(STM_READ_SIG(,,));
      static TM_FASTCALL void* read_rw(STM_READ_SIG(,,));
      static TM_FASTCALL void write_ro(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void write_rw(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void commit_ro(STM_COMMIT_SIG(,));
      static TM_FASTCALL void commit_rw(STM_COMMIT_SIG(,));
      static TM_FASTCALL void delay_ro(STM_DELAY_SIG(,,,));
      static TM_FASTCALL void delay_rw(STM_DELAY_SIG(,,,));
      static stm::scope_t* rollback(STM_ROLLBACK_SIG(,,,));
      static bool irrevoc(STM_IRREVOC_SIG(,));
      static void onSwitchTo();
  };

  /**
   *  These defines are for tuning backoff behavior
   */
#if defined(STM_CPU_SPARC)
#  define READ_TIMEOUT        32
#  define ACQUIRE_TIMEOUT     128
#  define DRAIN_TIMEOUT       1024
#else // STM_CPU_X86
#  define READ_TIMEOUT        32
#  define ACQUIRE_TIMEOUT     128
#  define DRAIN_TIMEOUT       256
#endif

  /**
   *  DelayByteEager begin:
   */
  bool
  DelayByteEager::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      return false;
  }

void DelayByteEager::delay_ro(STM_DELAY_SIG(tx,input,output,fn)){
    stm::fntuple_t tuple;
    tuple.input_index = input;
    tuple.output_index = output;
    tuple.fn = fn;
    tx->fn_list.insert(tuple);
    return;
}
void DelayByteEager::delay_rw(STM_DELAY_SIG(tx,input,output,fn)){
    stm::fntuple_t tuple;
    tuple.input_index = input;
    tuple.output_index = output;
    tuple.fn = fn;
    tx->fn_list.insert(tuple);
    return;
}

  /**
   *  DelayByteEager commit (read-only):
   */
  void
  DelayByteEager::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // read-only... release read locks
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      tx->r_bytelocks.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  DelayByteEager commit (writing context):
   */
  void
  DelayByteEager::commit_rw(STM_COMMIT_SIG(tx,))
  {
      // release write locks, then read locks
      foreach (ByteLockList, i, tx->w_bytelocks)
          (*i)->owner = 0;
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // clean-up
      tx->r_bytelocks.reset();
      tx->w_bytelocks.reset();
      tx->undo_log.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  DelayByteEager read (read-only transaction)
   */
  void*
  DelayByteEager::read_ro(STM_READ_SIG(tx,addr,))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

      // do I have a read lock?
      if (lock->reader[tx->id-1] == 1)
          return *addr;

      // log this location
      tx->r_bytelocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader byte
          lock->set_read_byte(tx->id-1);

          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return *addr;

          // drop read lock, wait (with timeout) for lock release
          lock->reader[tx->id-1] = 0;
          while (lock->owner != 0) {
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
          }
      }
  }

  /**
   *  DelayByteEager read (writing transaction)
   */
  void*
  DelayByteEager::read_rw(STM_READ_SIG(tx,addr,))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

      // do I have the write lock?
      if (lock->owner == tx->id)
          return *addr;

      // do I have a read lock?
      if (lock->reader[tx->id-1] == 1)
          return *addr;

      // log this location
      tx->r_bytelocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader byte
          lock->set_read_byte(tx->id-1);
          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return *addr;

          // drop read lock, wait (with timeout) for lock release
          lock->reader[tx->id-1] = 0;
          while (lock->owner != 0)
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
      }
  }

  /**
   *  DelayByteEager write (read-only context)
   */
  void
  DelayByteEager::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

      // get the write lock, with timeout
      while (!bcas32(&(lock->owner), 0u, tx->id))
          if (++tries > ACQUIRE_TIMEOUT)
              tx->tmabort(tx);

      // log the lock, drop any read locks I have
      tx->w_bytelocks.insert(lock);
      lock->reader[tx->id-1] = 0;

      // wait (with timeout) for readers to drain out
      // (read 4 bytelocks at a time)
      volatile uint32_t* lock_alias = (volatile uint32_t*)&lock->reader[0];
      for (int i = 0; i < 15; ++i) {
          tries = 0;
          while (lock_alias[i] != 0)
              if (++tries > DRAIN_TIMEOUT)
                  tx->tmabort(tx);
      }

      // add to undo log, do in-place write
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);

      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  DelayByteEager write (writing context)
   */
  void
  DelayByteEager::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

      // If I have the write lock, add to undo log, do write, return
      if (lock->owner == tx->id) {
          tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
          STM_DO_MASKED_WRITE(addr, val, mask);
          return;
      }

      // get the write lock, with timeout
      while (!bcas32(&(lock->owner), 0u, tx->id))
          if (++tries > ACQUIRE_TIMEOUT)
              tx->tmabort(tx);

      // log the lock, drop any read locks I have
      tx->w_bytelocks.insert(lock);
      lock->reader[tx->id-1] = 0;

      // wait (with timeout) for readers to drain out
      // (read 4 bytelocks at a time)
      volatile uint32_t* lock_alias = (volatile uint32_t*)&lock->reader[0];
      for (int i = 0; i < 15; ++i) {
          tries = 0;
          while (lock_alias[i] != 0)
              if (++tries > DRAIN_TIMEOUT)
                  tx->tmabort(tx);
      }

      // add to undo log, do in-place write
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);
  }

  /**
   *  DelayByteEager unwinder:
   */
  stm::scope_t*
  DelayByteEager::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Undo the writes, while at the same time watching out for the exception
      // object.
      STM_UNDO(tx->undo_log, upper_stack_bound, except, len);

      // release write locks, then read locks
      foreach (ByteLockList, i, tx->w_bytelocks)
          (*i)->owner = 0;
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // reset lists
      tx->r_bytelocks.reset();
      tx->w_bytelocks.reset();
      tx->undo_log.reset();

      // randomized exponential backoff
      exp_backoff(tx);

      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  DelayByteEager in-flight irrevocability:
   */
  bool DelayByteEager::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  Switch to DelayByteEager:
   */
  void DelayByteEager::onSwitchTo() {
  }
}

namespace stm {
  /**
   *  DelayByteEager initialization
   */
  template<>
  void initTM<DelayByteEager>()
  {
      // set the name
      stms[DelayByteEager].name      = "DelayByteEager";

      // set the pointers
      stms[DelayByteEager].begin     = ::DelayByteEager::begin;
      stms[DelayByteEager].commit    = ::DelayByteEager::commit_ro;
      stms[DelayByteEager].read      = ::DelayByteEager::read_ro;
      stms[DelayByteEager].write     = ::DelayByteEager::write_ro;
      stms[DelayByteEager].rollback  = ::DelayByteEager::rollback;
      stms[DelayByteEager].irrevoc   = ::DelayByteEager::irrevoc;
      stms[DelayByteEager].switcher  = ::DelayByteEager::onSwitchTo;
      stms[DelayByteEager].delay     = ::DelayByteEager::delay_ro ;
      stms[DelayByteEager].privatization_safe = true;
  }
}
