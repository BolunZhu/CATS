///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007, 2008, 2009
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

#ifndef FOREST_HPP__
#define FOREST_HPP__

#include <stm/stm.hpp>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include "DList.hpp"
#include "RBTree.hpp"
#ifdef _MSC_VER
#include <alt-license/rand_r.h>
#endif

namespace bench
{
  /**
   *  Another benchmark to try to get things to livelock.  here, we don't
   *  usually have write-write conflicts, but we hope to have read-write
   *  conflicts.
   *
   *  Note: to use this benchmark, construct it with string of the form
   *  X(-Y,Z)*.  X is the number of random tree operations to perform.  The
   *  (-Y,Z) parameters each correspond to a single tree, with Y being the max
   *  key and Z being the lookup percentage.  For example 8-256,22-256,33 would
   *  indicate that 8 operations should be performed on a forest of two trees,
   *  where one tree has keys in the range 0..255 and has a 22% lookup ratio
   *  (thus 39% insert and 39% remove), and the other tree also has keys in the
   *  range 0..255 but with a 33% lookup ratio (thus 33% insert and 33%
   *  remove).
   */
  class Forest : public Benchmark
  {
      // how many keys in each tree?
      std::vector<int> keydepths;
      // lookup ratio for each tree
      std::vector<int> ratios;
      // random trees to touch per transaction
      int trees_per_tx;
      // total number of trees
      int total_trees;
      // set of trees
      std::vector<bench::RBTree*> trees;

    public:
      // constructor: create all the trees
      Forest(std::string configstr)
      {
          std::string substr = configstr;
          // first cut out the trees_per_tx value
          int t1 = substr.find_first_of('-', 0);
          int t2 = substr.size();
          trees_per_tx = atoi(substr.substr(0, t1).c_str());
          substr = substr.substr(t1+1, t2-t1).c_str();
          while (true) {
              // chop up the config string by '-'
              int pos1 = substr.find_first_of('-', 0);
              int pos2 = substr.size();
              // cut is the first x,y pair
              std::string cut = substr.substr(0, pos1).c_str();
              // within cut, split by ','
              int q1 = cut.find_first_of(',', 0);
              int q2 = cut.size();
              // convert two halves into ints
              int p1 = atoi(cut.substr(0, q1).c_str());
              int p2 = atoi(cut.substr(q1+1, q2-q1).c_str());
              // store portions into appropriate arrays
              keydepths.push_back(p1);
              ratios.push_back(p2);
              // advance to next pair
              substr = substr.substr(pos1+1, pos2-pos1).c_str();
              if (pos1 == -1)
                  break;
          }
          // now for each value in keydepths, create a tree
          std::vector<int>::iterator i;
          for (i = keydepths.begin(); i < keydepths.end(); ++i) {
              // make new tree
              RBTree* nexttree = new RBTree();
              // populate it
              for (int w = 0; w < *i; w+=2) {
                  nexttree->insert(w);
              }
              // add it to the tree vector
              trees.push_back(nexttree);
          }
          total_trees = keydepths.size();
      }

      // threads either increment from front to back or from back to front,
      // based on ID.  However, threads don't increment everything, so we
      // shouldn't see write-write conflicts, and hopefully we avoid conflicts
      // before validation time
      //
      // NB: use args->id to get the thread id
      virtual void random_transaction(thread_args_t* args,
                                      unsigned int*  seed,
                                      unsigned int   val,
                                      int            action)
      {
          int * whichtrees = new int[trees_per_tx];
          int * vals = new int[trees_per_tx];
          int * actions = new int[trees_per_tx];

          for (int i = 0; i < trees_per_tx; i++) {
              whichtrees[i] = rand_r(seed) % total_trees;
              vals[i] = rand_r(seed) % keydepths[whichtrees[i]];
              actions[i] = rand_r(seed) % 100;
          }

          BEGIN_TRANSACTION {
              // subsumption nesting with conditional to break out of livelock
              // situations when the timer goes off
              if (!bench::early_tx_terminate) {
                  for (int i = 0; i < trees_per_tx; i++) {
                      int lr = ratios[whichtrees[i]];
                      if (actions[i] < lr) {
                          trees[whichtrees[i]]->lookup(vals[i]);
                      }
                      else if (actions [i] < (((100 - lr)/2)+lr)) {
                          trees[whichtrees[i]]->insert(vals[i]);
                      }
                      else {
                          trees[whichtrees[i]]->remove(vals[i]);
                      }
                  }
              }
          } END_TRANSACTION;

          delete whichtrees;
          delete vals;
          delete actions;
      }

      // make sure the list is in sorted order
      virtual bool sanity_check() const
      {
          std::vector<bench::RBTree*>::const_iterator i;
          for (i = trees.begin(); i < trees.end(); ++i) {
              if (!(*i)->isSane())
                  return false;
          }
          return true;
      }

      // no data structure verification is implemented for the PrivList yet
      virtual bool verify(VerifyLevel_t v) { return true; }
  };

} // namespace bench

#endif // FOREST_HPP__
