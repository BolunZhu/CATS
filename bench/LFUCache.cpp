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

#include <stm/stm.hpp>
#include "LFUCache.hpp"

#ifdef _MSC_VER
#include <alt-license/rand_r.h>
#endif

using namespace stm;

namespace bench
{
    static const int LOG_TIGHTRANGE = 8;
    static const int TIGHTRANGE = (1 << LOG_TIGHTRANGE);
    static const int TIGHTRANGE_MASK = (TIGHTRANGE - 1);

    static const int tableSize = 2048;
    static const int heapDepth = 8;                 // 255 entries
    static const int heapSize = ((1 << (heapDepth + 1)) - 1);

    // defines each node in the priority heap of the LFUCache
    class PriorityHeapNode : public Object
    {
        GENERATE_FIELD(int, value);
        GENERATE_FIELD(int, frequencyCount);
        GENERATE_FIELD(sh_ptr<PriorityHeapNode>, left);
        GENERATE_FIELD(sh_ptr<PriorityHeapNode>, right);
      public:

        PriorityHeapNode(int _value = 0,
                         int _frequencyCount = 0,
                         PriorityHeapNode* _left = NULL,
                         PriorityHeapNode* _right = NULL)
            : m_value(_value),
              m_frequencyCount(_frequencyCount),
              m_left(_left),
              m_right(_right)
        { }

        void swapDataWith(wr_ptr<PriorityHeapNode> other_w,
                          wr_ptr<PriorityHeapNode> this_w)
        {
            // Swap frequency count
            int tmp = this_w->get_frequencyCount(this_w);
            this_w->set_frequencyCount(other_w->get_frequencyCount(other_w),
                                       this_w);
            other_w->set_frequencyCount(tmp, other_w);

            // Swap node value
            tmp = this_w->get_value(this_w);
            this_w->set_value(other_w->get_value(other_w),
                              this_w);
            other_w->set_value(tmp, other_w);

            // Leave structural data unchanged
        }
    };


    // table entries point to elements in the heap
    class TableEntry : public Object
    {
        GENERATE_FIELD(int, value);
        GENERATE_FIELD(sh_ptr<PriorityHeapNode>, heapPtr);
      public:

        TableEntry(int _value = 0)
            : m_value(_value),
              m_heapPtr()
        { }
    };


    // The LFUCache has a heap and a table of values
    class LFUCache
    {
      private:
        sh_ptr<PriorityHeapNode> heap;
        sh_ptr<TableEntry>* table;

        // build the initial heap
        void buildHeap()
        {
            // Top node of the heap is really just a dummy sentinel node.
            // The true heap is contained as the sentinel's left child.
            heap = sh_ptr<PriorityHeapNode>
                (new PriorityHeapNode(-1, 0, buildSubHeap(heapDepth), NULL));
        }

        // recursively portion of heap creation
        PriorityHeapNode* buildSubHeap(int depthRemaining)
        {
            // Halt recursion at depth 0
            if (0 == depthRemaining)
                return NULL;

            // Build a dummy node with children recursively populated
            // under it
            return new PriorityHeapNode(-1, 0,
                                        buildSubHeap(depthRemaining - 1),
                                        buildSubHeap(depthRemaining - 1));
        }

        // build the element table
        void buildElementTable()
        {
            table = new sh_ptr<TableEntry>[tableSize];

            for (int j = 0; j < tableSize; j++) {
                table[j] = sh_ptr<TableEntry>(new TableEntry(j));
            }
        }

      public:
        LFUCache()
        {
            buildHeap();
            buildElementTable();
        }

        // return the size of the table
        int getElementCount() const {
            return tableSize;
        }

        // hit a page, maybe change the heap
        void pageHit(int v)
        {
            BEGIN_TRANSACTION;
            if (!bench::early_tx_terminate) {

            wr_ptr<TableEntry> elt(table[v]);
            wr_ptr<PriorityHeapNode> node;

            // Are we updating an element already in the heap?
            if (NULL != elt->get_heapPtr(elt)) {
                node = elt->get_heapPtr(elt);
                node->set_frequencyCount(node->get_frequencyCount(node)+1,
                                         node);
            }
            // otherwise we are adding a new element to the heap
            else {
                rd_ptr<PriorityHeapNode> sentinel(heap);
                node = sentinel->get_left(sentinel);

                // Kick out the old root
                if (-1 != node->get_value(node))
                {
                    wr_ptr<TableEntry> oldRootElt
                        (table[node->get_value(node)]);

                    oldRootElt->set_heapPtr(wr_ptr<PriorityHeapNode>(),
                                            oldRootElt);
                }

                // Stuff in the new data
                node->set_value(v, node);
                node->set_frequencyCount(1, node);
            }

            // re-heapify based on frequency count
            // Note that we always swap with a single-hit node so as to
            // give new pages a chance to build up some frequency
            bool moved = false;

            // Pointers that would otherwise be initialized multiple times in
            // the do loop. There are upgrades to writable inside the loop
            // which are fine, because the transaction context can be copied.
            rd_ptr<PriorityHeapNode> left_r;
            rd_ptr<PriorityHeapNode> right_r;

            do {
                moved = false;

                if (NULL == node->get_left(node))
                    break; // reached bottom layer

                left_r = node->get_left(node);
                right_r = node->get_right(node);

                // Potentially swap with the least-used child
                if (left_r->get_frequencyCount(left_r) <
                    right_r->get_frequencyCount(right_r))
                {
                    if ((left_r->get_frequencyCount(left_r) <
                         node->get_frequencyCount(node))
                        || (left_r->get_frequencyCount(left_r) == 1))
                    {
                        // Swap data
                        wr_ptr<PriorityHeapNode> left(left_r);
                        left->swapDataWith(node, left);

                        // Update table pointers
                        if (-1 != node->get_value(node))
                        {
                            wr_ptr<TableEntry> otherElt
                                (table[node->get_value(node)]);

                            otherElt->set_heapPtr(elt->get_heapPtr(elt),
                                                  otherElt);
                        }

                        elt->set_heapPtr(node->get_left(node),
                                         elt);
                        node = left;
                        moved = true;
                    }
                }
                else {
                    // Swap with right child?
                    if ((right_r->get_frequencyCount(right_r) <
                         node->get_frequencyCount(node)) ||
                        (right_r->get_frequencyCount(right_r) == 1))
                    {
                        // Swap data
                        wr_ptr<PriorityHeapNode> right(right_r);
                        right->swapDataWith(node, right);

                        // Update table pointers
                        if (-1 != node->get_value(node))
                        {
                            wr_ptr<TableEntry> otherElt
                                (table[node->get_value(node)]);

                            otherElt->set_heapPtr(elt->get_heapPtr(elt),
                                                  otherElt);
                        }

                        elt->set_heapPtr(node->get_right(node), elt);
                        node = right;
                        moved = true;
                    }
                }
            }
            while (moved);
            }
            END_TRANSACTION;
        }

        // check the integrity of the LFUCache
        bool isSane() const
        {
            bool sane = false;

            BEGIN_TRANSACTION;

            sane = true;

            // Pointers that would otherwise be initialized in the loop
            rd_ptr<TableEntry> elt;
            rd_ptr<PriorityHeapNode> heap;
            rd_ptr<PriorityHeapNode> heap_lr;

            for (int j = 0; j < getElementCount(); j++) {
                elt = table[j];

                if (elt->get_heapPtr(elt) != NULL) {
                    heap = elt->get_heapPtr(elt);

                    if (heap->get_left(heap) != NULL) {
                        heap_lr = heap->get_left(heap);

                        if (heap_lr->get_frequencyCount(heap_lr) <
                            heap->get_frequencyCount(heap))
                        {
                            sane = false;
                            break;
                        }
                    }

                    if (heap->get_right(heap) != NULL) {
                        heap_lr = heap->get_right(heap);

                        if (heap_lr->get_frequencyCount(heap_lr) <
                            heap->get_frequencyCount(heap))
                        {
                            sane = false;
                            break;
                        }
                    }
                }
            }

            END_TRANSACTION;

            return sane;
        }
    };

    static const int randomRange = 1000000;
    static int* zipfLUT;
    static bool LUTcreated = false;
}


using namespace bench;

// Build the test harness
LFUTest::LFUTest()
{
    cache = new LFUCache();
    if (!LUTcreated) {
        zipfLUT = new int[cache->getElementCount()];

        // Build the ZIPF distribution (pow 2.0 for this version), normalizing
        // the discretized probabilities to our random number generation range
        double tot, i, sum = 0.0;
        for (i = 1.0; i <= cache->getElementCount(); i++)
            sum += 1.0/(i * i);
        tot = double(randomRange)/sum;
        sum = 0.0;
        for (i = 1.0; i <= cache->getElementCount(); i++) {
            sum += 1.0/(i * i);
            zipfLUT[int(i) - 1] = int(tot * sum + 0.5);
        }
        LUTcreated = true;
    }
}

// test by executing a transaction
void LFUTest::random_transaction(thread_args_t* args, unsigned int* seed,
                                 unsigned int val,    int chance)
{
    int discProb, page;

    // Pick the next page we want, using the discretized probabilities from the
    // pre-calculated ZIPF distribution-> Note that we don't use binary search
    // because the distribution is exponential
    discProb = rand_r(seed) % randomRange;
    for (page = 0; page < cache->getElementCount(); page++) {
        if (discProb < zipfLUT[page]) break;
    }
    if (page == cache->getElementCount()) page--;

    // hit the page
    cache->pageHit(page);
}

// make sure the heap is still sane
bool LFUTest::sanity_check() const
{
    return cache->isSane();
}
