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

#ifndef MINIVECTOR_HPP__
#define MINIVECTOR_HPP__

#include <cassert>

namespace stm
{
  /**
   *  Simple self-growing array.  This is a lot like the stl vector, except
   *  that we don't destroy list elements when we clear the list, resulting in
   *  an O(1) clear() instead of the standard stl O(n) clear overhead.
   *
   *  A lot of thought went into this data structure, even if it doesn't seem
   *  like it.  std::vector and our old stm::SearchList data structures both
   *  perform worse.
   */
  template <class T>
  class MiniVector
  {
    protected:
      unsigned long m_cap;            /// current vector capacity
      unsigned long m_size;           /// current number of used elements
      T* m_elements;                  /// the actual elements in the vector

    public:

      /** iterator interface, just use a basic pointer */
      typedef T* iterator;

      /** Construct a minivector with a default size */
      MiniVector(const unsigned long capacity)
          : m_cap(capacity), m_size(0),
            m_elements(static_cast<T*>(malloc(sizeof(T)*m_cap)))
      {
          assert(m_elements);
      }

      /** Reset the vector without destroying the elements it holds */
      void reset()
      {
          m_size = 0;
      }

      /** Insert an element into the minivector */
      void insert(T data)
      {
          // NB: There is a tradeoff here.  If we put the element into the list
          // first, we are going to have to copy one more object any time we
          // double the list.  However, by doing things in this order we avoid
          // constructing /data/ on the stack if (1) it has a simple
          // constructor and (2) /data/ isn't that big relative to the number
          // of available registers.

          // Push data onto the end of the array and increment the size
          m_elements[m_size++] = data;

          // If the list is full, double the list size, allocate a new array
          // of elements, bitcopy the old array into the new array, and free
          // the old array. No destructors are called.
          if (m_size == m_cap) {
              T* temp = m_elements;

              m_cap *= 2;

              m_elements = static_cast<T*>(malloc(sizeof(T)*m_cap));
              assert(m_elements);

              memcpy(m_elements, temp, sizeof(T)*m_size);

              free(temp);
          }
      }

      /**
       * Delete by index. This does not call any destructors, and is just a
       * simple move. This /will/ call an operator= if one exists.
       */
      void remove(const unsigned long i)
      {
          // If we're not removing the last element, copy the last element
          // into the element at index and decrement the size. Array index
          // rather than pointer arithmatic because it helps the compiler
          // (possibly).
          if (--m_size != i)
              m_elements[i] = m_elements[m_size];
      }

      /**
       * Iterator based removal, just use pointer arithmetic and forward to
       * index based removal.
       */
      void remove(const iterator i)
      {
          assert((unsigned long)(i - m_elements) < m_size);
          remove(i - m_elements);
      }

      unsigned long size() const { return m_size; }
      /** Return true if size is 0. */
      bool is_empty() const { return !m_size; }
      iterator begin() const { return m_elements; }
      iterator end() const { return m_elements + m_size; }

  }; // template MiniVector
} // stm

#endif // MINIVECTOR_HPP__
