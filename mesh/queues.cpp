///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006, 2007, 2008, 2009
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

/*  queues.cc
 *
 *  Sequential and concurrent queues.
 *  Element type should always be a pointer.
 */

#include <stdlib.h>
#include <sys/mman.h>
#include "config.hpp"
#include "queues.hpp"

//////////////////////////////
//
//  Atomic ASM Operations

#include <stm/support/atomic_ops.h>        // From the RSTM package

//////////////////////////////
//
//  Get memory from the OS directly

inline void* alloc_mmap(unsigned size)
{
    void* mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
                     -1, 0);
    assert(mem != MAP_FAILED);
    return mem;
}

//////////////////////////////
//
//  macro to update counted pointers
//  Arguments are
//  cp address of counted pointer
//  op old pointer value expected to be there
//  sn serial number expected to be there
//  np new pointer value we want to put there

#define cp_CAS(cp,op,sn,np) \
    casX((volatile unsigned long long*)(cp), \
         (unsigned long)(op), (unsigned long)(sn), \
         (unsigned long)(np), (unsigned long)((sn)+1))

//////////////////////////////
//
//  Block Pool

struct qnode_t {
    void* t;
    counted_ptr next;
};

class block_pool
{
    struct shared_block_t
    {
        struct shared_block_t* volatile next;
        struct shared_block_t* volatile next_group;
        volatile qnode_t payload;
    };

    struct block_head_node_t
    {
        shared_block_t* volatile top;    // top of stack
        shared_block_t* volatile nth;    // ptr to GROUP_SIZE+1-th node, if
                                         // any, counting up from the bottom of
                                         // the stack
        volatile unsigned long count;    // number of nodes in list
    } __attribute__((aligned(CACHELINESIZE)));

    // add and remove blocks from/to global pool in clumps of this size
    static const unsigned long GROUP_SIZE = 8;

    static const int blocksize = sizeof(shared_block_t);

    // [?] why don't we make this a static member, align it, make it volatile,
    // and then pass its address to cp_cas?
    // MLS: not safe if there's more than one block pool
    counted_ptr* global_pool;

    block_head_node_t* head_nodes;  // one per thread

    // The following is your typical memory allocator hack.  Callers care about
    // payloads, which may be of different sizes in different pools.  Pointers
    // returned from alloc_block and passed to free_block point to the payload.
    // The bookkeeping data (next and next_group) are *in front of* the
    // payload.  We find them via pointer arithmetic.
    static inline shared_block_t* make_shared_block_t(void* block)
    {
        return (shared_block_t*)(((unsigned char*)block) -
                                 ((unsigned long)&(((shared_block_t*)0)->
                                                   payload)));
    }

public:

    // make sure nothing else is in the same line as this object
    static void* operator new(size_t size) { return alloc_mmap(size); }
    // [?] why don't we define an operator delete to call munmap()?

    block_pool(int numthreads);

    void* alloc_block(int tid);
    void free_block(void* block, int tid);
};

//  Create and return a pool to hold blocks of a specified size, to be shared
//  by a specified number of threads.  Return value is an opaque pointer.  This
//  routine must be called by one thread only.
//
inline block_pool::block_pool(int _numthreads)
{
    // get memory for the head nodes
    head_nodes = (block_head_node_t*)
        memalign(CACHELINESIZE, _numthreads * sizeof(block_head_node_t));
    // get memory for the global pool
    global_pool = (counted_ptr*)memalign(CACHELINESIZE, CACHELINESIZE);

    // make sure the allocations worked
    assert(head_nodes != 0 && global_pool != 0);

    // zero out the global pool
    global_pool->p.ptr = 0;
    global_pool->p.sn = 0;       // not really necessary

    // configure the head nodes
    for (int i = 0; i < _numthreads; i++) {
        block_head_node_t* hn = &head_nodes[i];
        hn->top = hn->nth = 0;
        hn->count = 0;
    }
}

inline void* block_pool::alloc_block(int tid)
{
    block_head_node_t* hn = &head_nodes[tid];
    shared_block_t* b = hn->top;
    if (b) {
        hn->top = b->next;
        hn->count--;
        if (b == hn->nth)
            hn->nth = 0;
    }
    else {
        // local pool is empty
        while (true) {
            counted_ptr gp;
            mvx(&global_pool->all, &gp.all);
            if ((b = (shared_block_t*)gp.p.ptr)) {
                unsigned long sn = gp.p.sn;
                if (cp_CAS(global_pool, b, sn, b->next_group)) {
                    // successfully grabbed group from global pool
                    hn->top = b->next;
                    hn->count = GROUP_SIZE-1;
                    break;
                }
                // else somebody else got into timing window; try again
            }
            else {
                // global pool is empty
                b = (shared_block_t*)memalign(CACHELINESIZE, blocksize);
                assert(b != 0);
                break;
            }
        }
        // In real-time code I might want to limit the number of iterations of
        // the above loop, and go ahead and malloc a new node when there is
        // very heavy contention for the global pool.  In practice I don't
        // expect a starvation problem.  Note in particular that the code as
        // written is preemption safe.
    }
    return (void*)&b->payload;
}

inline void block_pool::free_block(void* block, int tid)
{
    block_head_node_t* hn = &head_nodes[tid];
    shared_block_t* b = make_shared_block_t(block);

    b->next = hn->top;
    hn->top = b;
    hn->count++;
    if (hn->count == GROUP_SIZE+1) {
        hn->nth = hn->top;
    }
    else if (hn->count == GROUP_SIZE * 2) {
        // got a lot of nodes; move some to global pool
        shared_block_t* ng = hn->nth->next;
        while (true) {
            counted_ptr gp;
            unsigned long sn;
            mvx(&global_pool->all, &gp.all);
            b = (shared_block_t*)gp.p.ptr;
            sn = gp.p.sn;
            ng->next_group = b;
            if (cp_CAS(global_pool, b, sn, ng))
                break;
            // else somebody else got into timing window; try again
        }
        // In real-time code I might want to limit the number of iterations of
        // the above loop, and let my local pool grow bigger when there is very
        // heavy contention for the global pool.  In practice I don't expect a
        // problem.  Note in particular that the code as written is preemption
        // safe.
        hn->nth->next = 0;
        hn->nth = 0;
        hn->count -= GROUP_SIZE;
    }
}

//////////////////////////////
//
//  Templated Non-Blocking Queue

static block_pool* bp = 0;
    // Shared by all queue instances.  Note that all queue nodes are
    // the same size, because all payloads are pointers.

//  Following constructor should be called by only one thread, and there
//  should be a synchronization barrier (or something else that forces
//  a memory barrier) before the queue is used by other threads.
//  Paremeter is id of calling thread, whose subpool should be used to
//  allocate the initial dummy queue node.
//
MS_queue::MS_queue(const int tid)
{
    if (bp == 0) {
        // need to create block pool
        static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&lock);
        if (!bp)
            bp = new block_pool(num_workers);
        pthread_mutex_unlock(&lock);
    }
    qnode_t* qn = (qnode_t*)bp->alloc_block(tid);
    qn->next.p.ptr = 0;
    // leave qn->next.p.sn where it is!
    head.p.ptr = tail.p.ptr = qn;
    // leave head.p.sn and tail.p.sn where they are!
}

//  NB: Current implementation copies task structs into and out of queue.  This
//  is fine as long as tasks are small.  If they were large we might want to
//  consider an implementation that uses an extra level of indirection to avoid
//  the copy operations.
//
void MS_queue::enqueue(void* t, const int tid)
{
    qnode_t* qn = (qnode_t*)bp->alloc_block(tid);
    counted_ptr my_tail;

    qn->t = t;
    qn->next.p.ptr = 0;
    // leave sn where it is!
    while (true) {
        counted_ptr my_next;
        mvx(&tail.all, &my_tail.all);
        mvx(&((qnode_t*)my_tail.p.ptr)->next.all, &my_next.all);
        counted_ptr my_tail2;
        mvx(&tail.all, &my_tail2.all);
        if (my_tail.all == my_tail2.all) {
            // my_tail and my_next are mutually consistent
            if (my_next.p.ptr == 0) {
                // last node; try to link new node after this
                if (cp_CAS(&((qnode_t*)my_tail.p.ptr)->next,
                           my_next.p.ptr, my_next.p.sn, qn))
                {
                    break;              // enqueue worked
                }
            }
            else {
                // try to swing B->tail to next node
                (void)cp_CAS(&tail, my_tail.p.ptr,
                             my_tail.p.sn, my_next.p.ptr);
            }
        }
    }
    // try to swing B->tail to newly inserted node
    (void)cp_CAS(&tail, my_tail.p.ptr, my_tail.p.sn, qn);
}

// Returns 0 if queue was empty.  Since payloads are required to be
// pointers, this is ok.
//
void* MS_queue::dequeue(const int tid)
{
    counted_ptr my_head, my_tail;
    qnode_t* my_next;
    void* rtn;

    while (true) {
        mvx(&head.all, &my_head.all);
        mvx(&tail.all, &my_tail.all);
        my_next = (qnode_t*)((qnode_t*)my_head.p.ptr)->next.p.ptr;
        counted_ptr my_head2;
        mvx(&head.all, &my_head2.all);
        if (my_head.all == my_head2.all) {
            // head, tail, and next are mutually consistent
            if (my_head.p.ptr != my_tail.p.ptr) {
                // Read value out of node before CAS. Otherwise another dequeue
                // might free the next node.
                rtn = my_next->t;
                // try to swing head to next node
                if (cp_CAS(&head, my_head.p.ptr, my_head.p.sn, my_next)) {
                    break;                  // dequeue worked
                }
            }
            else {
                // queue is empty, or tail is falling behind
                if (my_next == 0)
                    // queue is empty
                    return 0;
                // try to swing tail to next node
                (void)cp_CAS(&tail, my_tail.p.ptr, my_tail.p.sn, my_next);
            }
        }
    }
    bp->free_block((void*)my_head.p.ptr, tid);
    return rtn;
}
