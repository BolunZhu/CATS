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

#ifndef HASH_HPP__
#define HASH_HPP__

#include <iostream>

#include "IntSet.hpp"
#include "LinkedList.hpp"

namespace bench
{

    static const int N_BUCKETS = 256;

    // the Hash class is an array of N_BUCKETS LinkedLists
    template
    <
        class ListType = LinkedList
    >
    class HashTable : public IntSet
    {
      private:

        /**
         *  Templated type defines what kind of list we'll use at each bucket.
         */
        ListType bucket[N_BUCKETS];


        /**
         *  during a sanity check, we want to make sure that every element in a
         *  bucket actually hashes to that bucket; we do it by passing this
         *  method to the extendedSanityCheck for the bucket.
         */
        static bool verify_hash_function(unsigned long val,
                                         unsigned long bucket)
        {
            return ((val % N_BUCKETS) == bucket);
        }


      public:

        virtual void insert(int val)
        {
            bucket[val % N_BUCKETS].insert(val);
        }


        virtual bool lookup(int val) const
        {
            return bucket[val % N_BUCKETS].lookup(val);
        }


        virtual void remove(int val)
        {
            bucket[val % N_BUCKETS].remove(val);
        }


        virtual bool isSane() const
        {
            for (int i = 0; i < N_BUCKETS; i++)
            {
                if (!bucket[i].extendedSanityCheck(verify_hash_function, i))
                {
                    return false;
                }
            }

            return true;
        }


        virtual void print() const
        {
            std::cout << "::Hash::" << std::endl;

            for (int i = 0; i < N_BUCKETS; i++)
            {
                bucket[i].print();
            }

            std::cout << "::End::" << std::endl;
        }
    };

    typedef HashTable<> Hash;

} // namespace bench

#endif // HASH_HPP__
