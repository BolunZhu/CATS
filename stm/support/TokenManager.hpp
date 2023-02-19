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

#ifndef __TOKENMANAGER_HPP__
#define __TOKENMANAGER_HPP__

#include <cassert>
#include "atomic_ops.h"
#include "defs.hpp"

namespace stm
{
  /**
   *  Very simple nonblocking token manager.  This has linear overhead, but we
   *  shouldn't be using it all that often
   */
  template <class T>
  class TokenManager
  {
      /*** Max number of tokens that can be given out / sizeof the array below */
      const int max_tokens;

      /*** Array of volatile pointers to nonvolatile <T>s */
      T* volatile * token_holders;

    public:

      /*** Create an array of [max] tokens, all unowned */
      TokenManager(int max) : max_tokens(max)
      {
          token_holders = (T* volatile *)malloc(max_tokens * sizeof(T*));
          for (int i = 0; i < max_tokens; i++)
              token_holders[i] = NULL;
      }

      /**
       *  O(n) algorithm to find a free token if one exists, and return it.
       *  Returns -1 on failure to find a token
       */
      int get_token(T* id)
      {
          for (int i = 0; i < max_tokens; i++) {
              if (token_holders[i] == 0) {
                  if (bool_cas((volatile unsigned long*)&token_holders[i],
                               0, (unsigned long)id))
                      return i;
              }
          }
          return -1;
      }

      /**
       * Returning a token is very easy: just set the position you own to
       * NULL.  Note that this can be very dangerous if it is used
       * incorrectly.
       */
      void return_token(int t) { token_holders[t] = NULL; }

      /**
       *  Simple lookup when you've got an index and need to know who holds
       *  the token
       */
      T* lookup(int index) const { return token_holders[index]; }

      /*** simple accessor (getter) for max_tokens */
      int get_max() const { return max_tokens; }
  };
} // stm

#endif // __TOKENMANAGER_HPP__
