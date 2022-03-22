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
using stm::fn_t;

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
      static TM_FASTCALL void* read_delay(STM_READ_SIG(,,));
      static TM_FASTCALL void write_delay(STM_WRITE_SIG(,,,));
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
  TM_INLINE
  inline uint32_t get_cnt_index_from_addr(TxThread* tx,uint64_t addr)
  {
      uintptr_t index = reinterpret_cast<uintptr_t>(addr);
      return (index>>3) % stm::NUM_STRIPES;
  }
  TM_INLINE
  inline uint32_t get_cnt_index_from_lock(TxThread* tx,bytelock_t* lock)
  {
      return (lock-stm::bytelocks);
  }
bool read_lock_ro(TxThread* tx,uint64_t addr){
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(reinterpret_cast<void*>(addr));

      // do I have a read lock?
      if (lock->reader[tx->id-1] == 1)
          return true;

      // log this location
      tx->r_bytelocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader byte
          lock->set_read_byte(tx->id-1);

          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return true;

          // drop read lock, wait (with timeout) for lock release
          lock->reader[tx->id-1] = 0;
          while (lock->owner != 0) {
              if (++tries > READ_TIMEOUT)
                  return false;
          }
      }
}

bool read_lock(TxThread* tx,uint64_t addr){
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(reinterpret_cast<void*>(addr));

      // do I have the write lock?
      if (lock->owner == tx->id)
          return true;

      // do I have a read lock?
      if (lock->reader[tx->id-1] == 1)
          return true;

      // log this location
      tx->r_bytelocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader byte
          lock->set_read_byte(tx->id-1);
          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return true;

          // drop read lock, wait (with timeout) for lock release
          lock->reader[tx->id-1] = 0;
          while (lock->owner != 0)
              if (++tries > READ_TIMEOUT)
                  return false;
      }
}
bool write_lock(TxThread* tx,uint64_t addr){
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(reinterpret_cast<void*>(addr));

      // If I have the write lock, add to undo log, do write, return
      if (lock->owner == tx->id) {
        //   tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
        //   STM_DO_MASKED_WRITE(addr, val, mask);
          return true;
      }

      // get the write lock, with timeout
      while (!bcas32(&(lock->owner), 0u, tx->id))
          if (++tries > ACQUIRE_TIMEOUT)
                return false;
            //   tx->tmabort(tx);


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
                  return false;
                //   tx->tmabort(tx);
      }

      // add to undo log, do in-place write
    //   tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
    //   STM_DO_MASKED_WRITE(addr, val, mask);
      return true;
}

void DelayByteEager::delay_ro(STM_DELAY_SIG(tx,input,output,fn)){
    //step1 lock
    for(uint32_t i=input; i<tx->input_tail; i++){
        uint64_t addr = tx->input_list[i];
        if(!read_lock_ro(tx,addr)){
            tx->tmabort(tx);
        }
    }
    for(uint32_t i=output; i<tx->output_tail; i++){
        uint64_t addr = tx->output_list[i];
        if(!write_lock(tx,addr)){
            tx->tmabort(tx);
        }
    }
    //step2 add cnts for bytelocks
    for(uint32_t i=input; i<tx->input_tail; i++){
        uint64_t addr = tx->input_list[i];
        tx->cnt_bytelocks[get_cnt_index_from_addr(tx,addr)]++;
    }
    for(uint32_t i=output; i<tx->output_tail; i++){
        uint64_t addr = tx->output_list[i];
        tx->cnt_bytelocks[get_cnt_index_from_addr(tx,addr)]++;
    }
    //step3 register tuples
    tx->fn_list[tx->fn_tail].input_index = input;
    tx->fn_list[tx->fn_tail].output_index = output;
    tx->fn_list[tx->fn_tail].fn = fn;
    tx->fn_tail++;
    return OnFirstWrite(tx, read_rw, write_rw, commit_rw,delay_rw);
}
void DelayByteEager::delay_rw(STM_DELAY_SIG(tx,input,output,fn)){
    
    for(uint32_t i=input; i<tx->input_tail; i++){
        uint64_t addr = tx->input_list[i];
        if(!read_lock(tx,addr)){
            tx->tmabort(tx);
        }
    }
    for(uint32_t i=output; i<tx->output_tail; i++){
        uint64_t addr = tx->output_list[i];
        if(!write_lock(tx,addr)){
            tx->tmabort(tx);
        }
    }
    //step2 add cnts for bytelocks
    for(uint32_t i=input; i<tx->input_tail; i++){
        uint64_t addr = tx->input_list[i];
        tx->cnt_bytelocks[get_cnt_index_from_addr(tx,addr)]++;
    }
    for(uint32_t i=output; i<tx->output_tail; i++){
        uint64_t addr = tx->output_list[i];
        tx->cnt_bytelocks[get_cnt_index_from_addr(tx,addr)]++;
    }
    //step3 register tuples
    tx->fn_list[tx->fn_tail].input_index = input;
    tx->fn_list[tx->fn_tail].output_index = output;
    tx->fn_list[tx->fn_tail].fn = fn;
    tx->fn_tail++;
    return;
}

// execute from fn_head to max_conflict_index before commmit phase
// can bu optimized since don't need to lock data
// for example: switch pointer tmread tmwrite and execute and switch back
void execute_delay_before_commit(TxThread* tx,uint32_t max_conflict_index){
    // printf("max_conflict_index=%d\n",max_conflict_index);
    tx->tmread = DelayByteEager::read_delay;
    tx->tmwrite = DelayByteEager::write_delay;
    for(uint32_t i=tx->fn_head; i<=max_conflict_index; i++){
    // for every delayed computation
        fn_t f = reinterpret_cast<fn_t>(tx->fn_list[i].fn);
        uint32_t input_index = tx->fn_list[i].input_index;
        uint32_t output_index = tx->fn_list[i].output_index;
        // execute
        f(tx,input_index,output_index);
        uint32_t next_input_index;
        uint32_t next_output_index;
        if (i+1==tx->fn_tail){
            next_input_index = tx->input_tail;
            next_output_index = tx->output_tail;
        }
        else{
            next_input_index = tx->fn_list[i+1].input_index;
            next_output_index = tx->fn_list[i+1].output_index;
        }
        // cnt-- for input
        for(uint32_t j=input_index; j<next_input_index;j++){
            tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->input_list[j])]--;
        }
        // cnt-- for output
        for(uint32_t j=output_index; j<next_output_index; j++){
            tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->output_list[j])]--;
        }
    }
    tx->tmread = DelayByteEager::read_rw;
    tx->tmwrite = DelayByteEager::write_rw;
    if(tx->fn_tail==max_conflict_index+1){ // max | tail
        // move to tail
        tx->fn_head=tx->fn_tail;
        tx->input_head=tx->input_tail;
        tx->output_head= tx->output_tail;
    }else{  // max | ... |active|... |tail
        // move to max_conflict_index
        tx->fn_head = max_conflict_index+1;
        tx->input_head = tx->fn_list[max_conflict_index+1].input_index;
        tx->output_head = tx->fn_list[max_conflict_index+1].output_index;
    }
    return;
}

// execute delayed computation in commit phase
void execute_delay_commit(TxThread * tx){
    
    tx->tmread = DelayByteEager::read_delay;
    tx->tmwrite = DelayByteEager::write_delay;
    // execute -> cnt--  -> if cnt==0 unlock
    for(uint32_t i=tx->fn_head; i<tx->fn_tail; i++){
        // for every delayed computation
        fn_t f = reinterpret_cast<fn_t>(tx->fn_list[i].fn);
        uint32_t input_index = tx->fn_list[i].input_index;
        uint32_t output_index = tx->fn_list[i].output_index;
        // execute
        f(tx,input_index,output_index);
        uint32_t next_input_index;
        uint32_t next_output_index;
        if (i+1==tx->fn_tail){
            next_input_index = tx->input_tail;
            next_output_index = tx->output_tail;
        }
        else{
            next_input_index = tx->fn_list[i+1].input_index;
            next_output_index = tx->fn_list[i+1].output_index;
        }
        // cnt-- and unlock if cnt==0 for input
        for(uint32_t j=input_index; j<next_input_index;j++){
            tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->input_list[j])]--;
            // printf("tx->cnt_bytelocks read %d=%d\n",j,
            // tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->input_list[j])]);
            if(tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->input_list[j])]==0){
                // read unlock
                // printf("unlock read %d\n",j);
                bytelock_t* lock = get_bytelock(reinterpret_cast<void*>(tx->input_list[j]));
                lock->reader[tx->id-1] = 0;
            }
        }
        // cnt-- and unlock if cnt==0 for output
        for(uint32_t j=output_index; j<next_output_index; j++){
            tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->output_list[j])]--;
            // printf("tx->cnt_bytelocks write %d=%d\n",j,
            // tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->output_list[j])]);
            if(tx->cnt_bytelocks[get_cnt_index_from_addr(tx,tx->output_list[j])]==0){
                // write unlock
                // printf("unlock write %d\n",j);
                bytelock_t* lock = get_bytelock(reinterpret_cast<void*>(tx->output_list[j]));
                lock->owner = 0;
            }
        }
    }
    // don't need to switch back cause they will be read_ro.. in the commit phase
    // tx->tmread = DelayByteEager::read_rw;
    // tx->tmwrite = DelayByteEager::write_rw;
    // printf("execute_delay_commit return with tx->fn_tail=%d\n",tx->fn_tail);
    tx->fn_head=0;
    tx->fn_tail=0;
    tx->input_head=0;
    tx->input_tail=0;
    tx->output_head=0;
    tx->output_tail=0;
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
      // release unregistered write locks, then read locks
      foreach (ByteLockList, i, tx->w_bytelocks){
          uint32_t index = get_cnt_index_from_lock(tx,*i);
          if(tx->cnt_bytelocks[index]==0){
              // not registered
            //   printf("unlock write\n");
              (*i)->owner = 0;
          }
      }

      foreach (ByteLockList, i, tx->r_bytelocks){
          uint32_t index = get_cnt_index_from_lock(tx,*i);
          if(tx->cnt_bytelocks[index]==0){
            //   printf("unlock read\n");
              (*i)->reader[tx->id-1] = 0;
          }
      }

    // execute delayed computation in order
    execute_delay_commit(tx);

      tx->r_bytelocks.reset();
      tx->w_bytelocks.reset();
      tx->undo_log.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro, delay_ro);
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

int32_t check_input(TxThread *tx, uint64_t addr){
    int64_t max_conflict_index=-1;
    for(uint32_t i= tx->input_head; i< tx->input_tail; i++){
        if(addr==tx->input_list[i]){
            max_conflict_index=i;
        }
    }
    if(max_conflict_index>=0){
        for(uint32_t i=tx->fn_head; i<tx->fn_tail; i++){
            if(tx->fn_list[i].input_index>max_conflict_index){
                return i-1;
            }
        }
        return tx->fn_tail-1;
    }else{ //not found
        return -1; 
    }
}
int32_t check_output(TxThread * tx, uint64_t addr){
    int64_t max_conflict_index=-1;
    for(uint32_t i=tx->output_head; i< tx->output_tail; i++){
        if(addr==tx->output_list[i]){
            max_conflict_index=i;
        }
    }
    if(max_conflict_index>=0){
        for(uint32_t i=tx->fn_head; i<tx->fn_tail; i++){
            if(tx->fn_list[i].output_index>max_conflict_index){
                return i-1;
            }
        }
        return tx->fn_tail;
    }else{
        return -1;
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
      {
          if(tx->cnt_bytelocks[get_cnt_index_from_addr(tx,reinterpret_cast<uint64_t>(addr))]>0){ //be registered
              int32_t res =check_output(tx,reinterpret_cast<uint64_t>(addr)); 
              if(res>=0){ //real conflict
                  execute_delay_before_commit(tx,(int32_t)res);
                  //execute all delayed from fn_head to max_conflict_index
              }
              // else: A and B share lock X. rester write A  -> read B
          }
          return *addr;
      }
          

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
          if(tx->cnt_bytelocks[get_cnt_index_from_addr(tx,reinterpret_cast<uint64_t>(addr))]>0){ //be registered
              int32_t res1 =check_output(tx,reinterpret_cast<uint64_t>(addr)); 
              int32_t res2 =check_input(tx,reinterpret_cast<uint64_t>(addr));
              int32_t res = res1>res2? res1: res2;
              if(res>=0){ //real conflict
                  execute_delay_before_commit(tx,(int32_t)res);
                  //execute all delayed from fn_head to max_conflict_index
              }
              // else: A and B share lock X. rester write A  -> read B
          }
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
      {//Check input address for WR conflict
          if(tx->cnt_bytelocks[get_cnt_index_from_addr(tx,reinterpret_cast<uint64_t>(addr))]>0){ //be registered
              int32_t res= check_input(tx,reinterpret_cast<uint64_t>(addr));
              if(res>=0){ //real conflict
                  execute_delay_before_commit(tx,(int32_t)res);
                  //execute all delayed from fn_head to max_conflict_index
              }
              // else: A and B share lock X. rester write A  -> read B
          }
      }
      // add to undo log, do in-place write
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);
  }
  void* DelayByteEager::read_delay(STM_READ_SIG(tx,addr,)){return *addr;}
  void DelayByteEager::write_delay(STM_WRITE_SIG(tx,addr,val,mask)){
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

     
      //clear all cnt
      for(uint32_t i=tx->input_head;i<tx->input_tail;i++){
          uint64_t addr = tx->input_list[i];
          tx->cnt_bytelocks[get_cnt_index_from_addr(tx,addr)]=0;
      }
      for(uint32_t i=tx->output_head;i<tx->output_tail;i++){
          uint64_t addr = tx->output_list[i];
          tx->cnt_bytelocks[get_cnt_index_from_addr(tx,addr)]=0;
      }
      tx->fn_head=0;
      tx->fn_tail=0;
      tx->input_head=0;
      tx->input_tail=0;
      tx->output_head=0;
      tx->output_tail=0;
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
