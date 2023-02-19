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

#ifndef STM_SUPPORT_WORD_BASE_WRITESET_HPP__
#define STM_SUPPORT_WORD_BASE_WRITESET_HPP__

#include <cstdlib>                      // memcpy
#include <cstring>                      // memset
#include "word_based_metadata.hpp"      // wlog_t

namespace stm
{
  class WriteSet
  {
    public:

      WriteSet(const size_t initial_capacity) :
          index(NULL),
          shift(8 * sizeof(void*)),
          ilength(0),
          version(1),                   // don't start with version 0
          list(NULL),
          capacity(initial_capacity),
          lsize(0)
      {
          // Find a good index length for the initial capacity of the list.
          while (ilength < 3 * initial_capacity)
              doubleIndexLength();

          index = new index_t[ilength];
          list  = (wlog_t*)malloc(sizeof(wlog_t) * capacity);
      }

      ~WriteSet()
      {
          delete[] index;
          free(list);
      }

      wlog_t* const find(void* const address) const
      {
          size_t h = hash(address);

          while (index[h].version == version)
              if (index[h].address == address)
                  return &list[index[h].index];
              else
                  h = (h + 1) % ilength;

          return NULL;
      }

      void insert(const wlog_t& log)
      {
          size_t h = hash(log.addr);

          // Find the slot where this address should hash to. If we find it,
          // update the value. If we find an unused slot then it's a new
          // insertion.
          while (index[h].version == version) {
              if (index[h].address == log.addr) {
                  list[index[h].index].val = log.val;
                  return;
              }
              else {
                  h = (h + 1) % ilength;
              }
          }

          // add the log to the list (guaranteed to have space)
          list[lsize] = log;

          // update the index
          index[h].address = log.addr;
          index[h].version = version;
          index[h].index   = lsize;

          // update the end of the list
          lsize += 1;

          // resize the list if needed
          if (lsize == capacity) {
              wlog_t* temp  = list;
              capacity     *= 2;
              list          = (wlog_t*)malloc(sizeof(wlog_t) * (capacity));
              memcpy(list, temp, sizeof(wlog_t) * lsize);
              free(temp);
          }

          // if we reach our load-factor
          // FIXME: load factor could be better handled rather than the magic
          //        constant 3 (used in constructor too).
          if ((lsize * 3) >= ilength) {
              rebuild();
          }
      }

      //---- Support the write set interface that the libraries use ----//

      const size_t size() const { return lsize; }

      void reset()
      {
          lsize    = 0;
          version += 1;

          // check overflow
          if (version == 0) {
              memset(index, 0, sizeof(index_t) * ilength);
              version = 1;
          }
      }

      typedef wlog_t* iterator;

      iterator begin() const { return list; }

      iterator end() const { return list + lsize; }

    private:
      // This doubles the size of the index. This *does not* do anything as
      // far as actually doing memory allocation. Callers should delete[] the
      // index table, increment the table size, and then reallocate it.
      const size_t doubleIndexLength()
      {
          assert(shift != 0 &&
                 "ERROR: the writeset doesn't support an index this large");
          shift   -= 1;
          ilength  = 1 << (8 * sizeof(void*) - shift);
          return ilength;
      }

      // hash function is straight from CLRS (that's where the magic constant
      // comes from)
      const size_t hash(void* const key) const
      {
          static const unsigned long long s = 2654435769ull;
          const unsigned long long r = ((unsigned long)key) * s;
          return (r & 0xFFFFFFFF) >> shift;
      }

      void rebuild()
      {
          assert(version != 0 && "ERROR: the version should *never* be 0");

          // extend the index
          delete[] index;
          index = new index_t[doubleIndexLength()];

          for (size_t i = 0; i < lsize; ++i) {
              const wlog_t& l = list[i];
              size_t h        = hash(l.addr);

              // search for the next available slot
              while (index[h].version == version)
                  h = (h + 1) % ilength;

              index[h].address = l.addr;
              index[h].version = version;
              index[h].index   = i;
          }
      }

      struct index_t
      {
          size_t version;
          void*  address;
          size_t index;

          index_t() : version(0), address(NULL), index(0) { }
      };

      index_t* index;
      size_t   shift;
      size_t   ilength;
      size_t   version;

      wlog_t*  list;
      size_t   capacity;
      size_t   lsize;
  };
}

#endif // STM_SUPPORT_WORD_BASE_WRITESET_HPP__
