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

#ifndef EXTENSIBLE_HASH_H__
#define EXTENSIBLE_HASH_H__

#ifndef STM_IGNORE_UNPTR_TESTS
#define STM_IGNORE_UNPTR_TESTS
#endif

#include <stm/stm.hpp>

#include <iostream>
using std::cout;
using std::endl;

/**
 *  Implement an extensible hash table that supports privatizing the table
 *  before rehashing
 */

template <typename KEY, class VALUE>
class ExtensibleHash
{
  private: // data types

    /**
     *  Each bucket of the hash table will be a list of these
     */
    class HashNode : public stm::Object
    {
        GENERATE_FIELD(KEY, key);
        GENERATE_FIELD(VALUE, value);
        GENERATE_FIELD(stm::sh_ptr<HashNode>, next);
      public:

        /**
         *  ctor when we have all fields
         */
        HashNode(KEY k, VALUE v, stm::sh_ptr<HashNode> n)
            : m_key(k), m_value(v), m_next(n)
        { }

        /**
         *  default ctor
         */
        HashNode() : m_key(-1), m_value(), m_next(NULL) { }
    };

    /*** wrapper for an unsigned long that we access transactionally */
    class ULONG : public stm::Object
    {
        GENERATE_FIELD(unsigned long, val);
      public:
        ULONG(unsigned long i = 0) : m_val(i) { }
    };

    /**
     * can't have an sh_ptr to an sh_ptr<>*, so we need this
     *
     * NB: this is a write-once field.  if it weren't, we'd need to have an
     * extra intermediate class to allow a hook for transactional writes and
     * reads to ptr[i]
     */
    class HashNodeRef : public stm::Object
    {
        GENERATE_FIELD(stm::sh_ptr<HashNode>*, ptr);
      public:
        HashNodeRef(stm::sh_ptr<HashNode>* v = 0) : m_ptr(v) { }
    };

  private: // fields

    /**
     *  Initial size
     */
    int start_size;

    /**
     *  Maximum bucket depth before rehashing
     */
    const unsigned max_depth;

    /**
     *  Flag for privatizing the whole data structure
     */
    stm::sh_ptr<ULONG> privatization_flag;

    /**
     *  Flag for indicating that a table needs to be rehashed
     */
    stm::sh_ptr<ULONG> need_rehash;

    /**
     *  Current number of buckets in the HashTable
     */
    stm::sh_ptr<ULONG> bucket_count;

    /**
     *  Set of buckets
     */
    stm::sh_ptr<HashNodeRef> buckets;

  private: // methods

    /**
     *  given a key and the number of buckets, choose a bucket.  Very simple
     *  for now
     */
    static long hash(KEY k, long buckets)
    {
        long z = (long)k;
        return z % buckets;
    }

    /**
     *  Helper method.  use un_ptrs to insert a <k,v> pair into a private hash
     *  table that has not been published yet.
     */
    static void unptr_insert(long new_count,
                             stm::sh_ptr<HashNode>* new_buckets,
                             KEY k,
                             VALUE v)
    {
        // compute bucket into which we should insert
        long bucket = hash(k, new_count);

        // insert into that bucket
        stm::un_ptr<HashNode> prev(new_buckets[bucket]);
        stm::un_ptr<HashNode> curr(prev->get_next(prev));

        // search for a key >= k
        while (curr != NULL) {
            // read pertinent fields of this node in a batch
            KEY rk = curr->get_key(curr);
            stm::sh_ptr<HashNode> rn = curr->get_next(curr);

            // if we found this key, big trouble
            if (rk == k) {
                assert(false);
                return;
            }

            // if we found a key > search key, break from loop
            if (rk > k)
                break;
            prev = curr;
            curr = rn;
        }

        // either curr is null or we found a key > k.  Insert new node
        prev->set_next(stm::sh_ptr<HashNode>(new HashNode(k, v, curr)), prev);
    }

  public: // methods

    /**
     *  Construct a non-privatized hash table with all buckets initialized
     */
    ExtensibleHash(long default_size, long initial_max_depth)
        : start_size(default_size),
          max_depth(initial_max_depth),
          privatization_flag(new ULONG(0)),
          need_rehash(new ULONG(0)),
          bucket_count(new ULONG(default_size)),
          buckets(new HashNodeRef((stm::sh_ptr<HashNode>*)
                                  stm::tx_alloc(default_size *
                                               sizeof(stm::sh_ptr<HashNode>))))
    {
        stm::un_ptr<HashNodeRef> root(buckets);
        stm::sh_ptr<HashNode>* p = root->get_ptr(root);
        // create a sentinel for each bucket
        for (int i = 0; i < default_size; i++) {
            p[i] = stm::sh_ptr<HashNode>(new HashNode());
        }
    }

    /**
     *  Attempt to insert a new key,value pair from within a transaction.  If
     *  rehash_point is nonzero, then the caller should try to rehash after
     *  completing this transaction, using the value of rehash_point.
     */
    bool insert_tx(KEY k, VALUE v)
    {
        // privatization guard
        stm::rd_ptr<ULONG> r_priv(privatization_flag);
        long priv = r_priv->get_val(r_priv);
        if (priv != 0)
            stm::retry();

        // get # of buckets
        stm::rd_ptr<ULONG> r_b(bucket_count);
        long b = r_b->get_val(r_b);

        // compute bucket into which we should insert
        long bucket = hash(k, b);

        // insert into that bucket
        stm::rd_ptr<HashNodeRef> r_root(buckets);
        stm::sh_ptr<HashNode>* p = r_root->get_ptr(r_root);
        stm::rd_ptr<HashNode> r_prev(p[bucket]);
        stm::rd_ptr<HashNode> r_curr(r_prev->get_next(r_prev));

        // count number of entries in this bucket
        unsigned long count = 0;

        // search for a key >= k
        while (r_curr != NULL) {
            count++;
            // read pertinent fields of this node in a batch
            KEY rk = r_curr->get_key(r_curr);
            stm::sh_ptr<HashNode> rn = r_curr->get_next(r_curr);

            // if we found this key, return false
            if (rk == k)
                return false;

            // if we found a key > search key, break from loop
            if (rk > k) {
                break;
            }

            r_prev = r_curr;
            r_curr = rn;
        }

        // either curr is null or we found a key > k.  Insert new node
        stm::wr_ptr<HashNode> w_prev(r_prev);
        w_prev->set_next(stm::sh_ptr<HashNode>(new HashNode(k, v, r_curr)),
                         w_prev);

        // do we need to rehash?  If so, set a flag and someone will take care
        // of it later
        if (max_depth <= count) {
            stm::wr_ptr<ULONG> rrehash(need_rehash);
            if (rrehash->get_val(rrehash) == 0) {
                stm::wr_ptr<ULONG> rehash(rrehash);
                rehash->set_val(1, rehash);
            }
        }
        return true;
    }

    /**
     *  Perform lookups from within a transaction, returns payload by reference
     */
    bool lookup_tx(KEY k, VALUE& v) const
    {
        // privatization guard
        stm::rd_ptr<ULONG> r_priv(privatization_flag);
        long priv = r_priv->get_val(r_priv);
        if (priv != 0)
            stm::retry();

        // get # of buckets
        stm::rd_ptr<ULONG> r_b(bucket_count);
        long b = r_b->get_val(r_b);

        // compute bucket into which we should look
        long bucket = hash(k, b);

        // search in that bucket
        stm::rd_ptr<HashNode> r_curr(buckets[bucket]);
        r_curr = r_curr->get_next(r_curr);

        // search for a key == k
        while (r_curr != NULL) {
            // read pertinent fields of this node in a batch
            KEY rk = r_curr->get_key(r_curr);
            VALUE rv = r_curr->get_value(r_curr);
            stm::sh_ptr<HashNode> rn = r_curr->get_next(r_curr);

            // if we found this key, set the reference and return true
            if (rk == k) {
                v = rv;
                return true;
            }
            // if we found a key > search key, return false
            if (rk > k)
                return false;
            // advance to next
            r_curr = rn;
        }

        return false;
    }

    // remove from the table
    bool remove(KEY k)
    {
        // privatization guard
        stm::rd_ptr<ULONG> r_priv(privatization_flag);
        long priv = r_priv->get_val(r_priv);
        if (priv != 0)
            stm::retry();

        // get # of buckets
        stm::rd_ptr<ULONG> r_b(bucket_count);
        long b = r_b->get_val(r_b);

        // compute bucket into which we should insert
        long bucket = hash(k, b);

        // insert into that bucket
        stm::rd_ptr<HashNode> r_prev(buckets[bucket]);
        stm::rd_ptr<HashNode> r_curr(r_prev->get_next(r_prev));

        // search for a key >= k
        while (r_curr != NULL) {
            // read pertinent fields of this node in a batch
            KEY rk = r_curr->get_key(r_curr);
            stm::sh_ptr<HashNode> rn = r_curr->get_next(r_curr);

            // if we found this key, disconnect it and end the search
            if (rk == k) {
                // disconnect
                stm::wr_ptr<HashNode> w_prev(r_prev);
                w_prev->set_next(rn, w_prev);

                // delete node
                stm::tx_delete(r_curr);
                return true;
            }


            // if we found a key > search key, return false
            if (rk > k)
                return false;

            // advance to next
            r_prev = r_curr;
            r_curr = rn;
        }

        // search failed
        return false;
    }

    /**
     *  Rehash via privatization
     */
    void reHash_privately()
    {
        privatize();

        // get # of buckets
        stm::un_ptr<ULONG> ubcount(bucket_count);
        long bcount = ubcount->get_val(ubcount);

        // compute new hashtable size
        long new_count = 2 * bcount;

        // allocate new table
        stm::sh_ptr<HashNode>* new_buckets =
            (stm::sh_ptr<HashNode>*)
            stm::tx_alloc(new_count * sizeof(stm::sh_ptr<HashNode>));
        for (int i = 0; i < new_count; i++) {
            new_buckets[i] = stm::sh_ptr<HashNode>(new HashNode());
        }

        // now transfer everything into the new table and free the old stuff
        stm::un_ptr<HashNodeRef> root(buckets);
        stm::sh_ptr<HashNode>* p = root->get_ptr(root);
        for (int i = 0; i < bcount; i++) {
            stm::un_ptr<HashNode> prev(p[i]);
            stm::un_ptr<HashNode> curr(prev->get_next(prev));
            stm::tx_delete(prev);
            while (curr != NULL) {
                // insert into new table
                unptr_insert(new_count, new_buckets,
                             curr->get_key(curr), curr->get_value(curr));
                prev = curr;
                curr = curr->get_next(curr);
                stm::tx_delete(prev);
            }
        }
        stm::tx_free(p);

        // now update the fields
        // -> bucket_count
        ubcount->set_val(new_count, ubcount);

        // -> buckets
        root->set_ptr(new_buckets, root);

        // reset rehash flag
        stm::un_ptr<ULONG> rehash(need_rehash);
        rehash->set_val(0, rehash);

        publish();
    }

    /***  Blindly rehash the table within a transaction*/
    void reHash_tx()
    {
        // get # of buckets
        stm::rd_ptr<ULONG> r_b(bucket_count);
        long bcount = r_b->get_val(r_b);

        // compute new hashtable size
        long new_count = 2 * bcount;

        // allocate new table
        stm::sh_ptr<HashNode>* new_buckets =
            (stm::sh_ptr<HashNode>*)
            stm::tx_alloc(new_count * sizeof(stm::sh_ptr<HashNode>));
        for (int i = 0; i < new_count; i++) {
            new_buckets[i] = stm::sh_ptr<HashNode>(new HashNode());
        }

        // now transfer everything into the new table and free the old stuff
        stm::rd_ptr<HashNodeRef> root(buckets);
        stm::sh_ptr<HashNode>* p = root->get_ptr(root);
        for (int i = 0; i < bcount; i++) {
            stm::rd_ptr<HashNode> prev(p[i]);
            stm::rd_ptr<HashNode> curr(prev->get_next(prev));
            stm::tx_delete(prev);
            while (curr != NULL) {
                // insert into new table
                unptr_insert(new_count, new_buckets,
                             curr->get_key(curr), curr->get_value(curr));
                prev = curr;
                curr = curr->get_next(curr);
                stm::tx_delete(prev);
            }
        }
        stm::tx_free(p);

        // write new hashtable size
        stm::wr_ptr<ULONG> w_b(r_b);
        w_b->set_val(new_count, w_b);

        // write the new pointer to the buckets
        stm::wr_ptr<HashNodeRef> wroot(root);
        wroot->set_ptr(new_buckets, wroot);

        // reset rehash flag
        stm::wr_ptr<ULONG> rehash(need_rehash);
        rehash->set_val(0, rehash);
    }

    /***  getter for need_rehash flag. */
    int needRehash()
    {
        int need = 0;
        BEGIN_TRANSACTION {
            need = 0;
            // read flag
            stm::rd_ptr<ULONG> rehash(need_rehash);
            int tmp = rehash->get_val(rehash);

            // read privatization guard
            stm::rd_ptr<ULONG> r_priv(privatization_flag);
            long was_private  = r_priv->get_val(r_priv);

            if (was_private == 0)
                need = tmp;
        } END_TRANSACTION;
        return need;
    }

    /***  getter for buckets in table. */
    int getBuckets()
    {
        int buckets = 0;
        BEGIN_TRANSACTION {
            buckets = 0;
            // read # elements
            stm::rd_ptr<ULONG> r_b(bucket_count);
            int tmp = r_b->get_val(r_b);

            // read privatization guard
            stm::rd_ptr<ULONG> r_priv(privatization_flag);
            long was_private  = r_priv->get_val(r_priv);

            if (was_private == 0)
                buckets = tmp;
        } END_TRANSACTION;
        return buckets;
    }

    /**
     *  Blindly privatize the table
     */
    bool privatize()
    {
        bool success;
        BEGIN_TRANSACTION{
            // read privatization guard
            stm::rd_ptr<ULONG> r_priv(privatization_flag);
            long was_private  = r_priv->get_val(r_priv);

            // upgrade if unset
            if (was_private == 0) {
                stm::wr_ptr<ULONG> w_priv(r_priv);
                w_priv->set_val(1, w_priv);
                success = true;
            }
            else {
                success = false;
            }
        } END_TRANSACTION;
        if (success)
            stm::acquire_fence();
        return success;
    }

    /**
     *  Blindly publish the table
     */
    void publish()
    {
        // verify table is privatized
        stm::un_ptr<ULONG> p(privatization_flag);
        assert(p->get_val(p) == 1);
        stm::release_fence();
        BEGIN_TRANSACTION{
            // unset guard
            stm::wr_ptr<ULONG> w_priv(privatization_flag);
            w_priv->set_val(0, w_priv);
        } END_TRANSACTION;
    }
};

#endif // EXTENSIBLE_HASH_H__
