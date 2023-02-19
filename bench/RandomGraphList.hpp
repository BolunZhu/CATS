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

#ifndef RANDOM_GRAPH_LIST_HPP__
#define RANDOM_GRAPH_LIST_HPP__

#include <stm/stm.hpp>
#include "Benchmark.hpp"

namespace bench
{
  /**
   * the RandomGraph consists of an ordered list of Vertex objects.  This list
   * is made of VListNodes.  A VListNode holds an sh_ptr to a Vertex, and a
   * next pointer.
   *
   * each Vertex consistes of two fields.  The first field is an int, which
   * uniquely identifies the Vertex.  The second field is a list (alist) of
   * adjacent nodes.  Since the adjacency list is simply an ordered list of
   * Vertex objects, we can use the VListNode for it.
   */

  class Vertex;

  /***  node type for an ordered list of Vertex objects */
  class VListNode : public stm::Object
  {
      GENERATE_FIELD(stm::sh_ptr<Vertex>,    vertex);
      GENERATE_FIELD(stm::sh_ptr<VListNode>, next);

    public:
      VListNode() : m_vertex(), m_next() { }

      VListNode(stm::sh_ptr<Vertex> v, stm::sh_ptr<VListNode> l)
          : m_vertex(v), m_next(l)
      { }
  };

  /***  vertex type */
  class Vertex : public stm::Object
  {
      GENERATE_FIELD(int, val);
      GENERATE_FIELD(stm::sh_ptr<VListNode>, alist);

    public:
      Vertex(int v = -1) : m_val(v), m_alist(new VListNode()) { }
  };

  /***  here's the graph type */
  class RandomGraphList
  {
      stm::sh_ptr<VListNode> nodes;
      const int maxNodes;
      const int default_linkups;

    public:

      RandomGraphList(int _maxNodes, int linkups = 4)
          : nodes(new VListNode()), maxNodes(_maxNodes),
            default_linkups(linkups)
      { }

      /**
       *  This is the entry point for an insert tx from the benchmark
       *  harness
       */
      void insert(int val, unsigned int* seed);

      /**
       *  and this is the entry point for a remove tx from the benchmark
       *  harness
       */
      void remove(int val);

      // sanity check
      bool isSane() const;

      // print
      void print() const;
  };

  class RGBench : public Benchmark
  {
      RandomGraphList* randomgraph;

    public:

      RGBench(int m) : randomgraph(new RandomGraphList(m)) { }

        // 50/50 mix of inserts and deletes
        void random_transaction(thread_args_t* args,
                                unsigned int* seed,
                                unsigned int val,
                                int chance)
        {
            if (chance % 2) {
                randomgraph->insert(val, seed);
                ++args->count[TXN_INSERT];
            }
            else {
                randomgraph->remove(val);
                ++args->count[TXN_REMOVE];
            }
        }

        // call the graph's sanity check
        bool sanity_check() const { return randomgraph->isSane(); }

        // no data structure verification is implemented for the RandomGraph
        // yet
        virtual bool verify(VerifyLevel_t v) { return true; }
    };
} // namespace bench

#endif // RANDOM_GRAPH_LIST_HPP__
