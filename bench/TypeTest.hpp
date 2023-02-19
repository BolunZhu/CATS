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

#ifndef TYPETEST_HPP__
#define TYPETEST_HPP__

#include <stm/stm.hpp>
#include <iostream>

using std::cout;
using std::endl;

namespace bench
{
  // test whether a word-based STM accesses various types correctly
  class TypeTest : public Benchmark
  {
      // TypeTestObject is an object with lots of different datatypes
      class TypeTestObject : public stm::Object
      {
          GENERATE_FIELD(char, cfield);
          GENERATE_FIELD(unsigned char, ucfield);
          GENERATE_FIELD(int, ifield);
          GENERATE_FIELD(unsigned int, uifield);
          GENERATE_FIELD(long, lfield);
          GENERATE_FIELD(unsigned long, ulfield);
          GENERATE_FIELD(unsigned, ufield);
          GENERATE_FIELD(float, ffield);
          GENERATE_FIELD(long long, llfield);
          GENERATE_FIELD(unsigned long long, ullfield);
          GENERATE_FIELD(double, dfield);

        public:

          TypeTestObject()
              : m_cfield('a'),
                m_ucfield(255),
                m_ifield(-2000000000L),
                m_uifield(4000000000UL),
                m_lfield(-2000000000L),
                m_ulfield(4000000000UL),
                m_ufield(4000000000UL),
                m_ffield(1.05f),
                m_llfield(-400000000000000000LL),
                m_ullfield(400000000000000000ULL),
                m_dfield(1.07)
          { }
      };
      stm::sh_ptr<TypeTestObject> tto;

    public:
      // constructor creates a single TypeTestObject
      TypeTest(int _whichtest) : tto(new TypeTestObject()) { }

      // NB: use args->id to get the thread id
      virtual void random_transaction(thread_args_t* args,
                                      unsigned int*  seed,
                                      unsigned int   val,
                                      int            chance)
      {
          // not a concurrent test...
          if (args->id == 0)
              DataTypeTest();
          ++args->count[TXN_INSERT];
      }

      // not implemented yet
      virtual bool sanity_check() const { return true; }

      /**
       *  This is just to make sure that all primitive data types are read,
       *  written, and read-after-written correctly.  You'll need to visually
       *  inspect the output to make sure everything is correct, unless you
       *  feel like running this in a debugger and watching that the correct
       *  templated versions of stm read and stm write are called, and that
       *  the values are being stored and retrieved correctly from the log
       *  types.
       *
       *  NB: this is only meaningful for word-based systems.  clone-based
       *  systems don't rely on the template code, so we don't need to verify
       *  it.
       */
      void DataTypeTest()
      {
          BEGIN_TRANSACTION {
              cout << "----------------------------" << endl;
              // why bother with a read pointer when we always write?
              stm::wr_ptr<TypeTestObject> wtto(tto);

              // test char and uchar
              char c = wtto->get_cfield(wtto);
              unsigned char uc = wtto->get_ucfield(wtto);
              char c2 = c + 1;
              unsigned char uc2= uc + 1;
              wtto->set_cfield(c2, wtto);
              wtto->set_ucfield(uc2, wtto);
              c2 = wtto->get_cfield(wtto);
              uc2 = wtto->get_ucfield(wtto);
              cout << "(c,uc) from ("
                   << (int)c << "," << (int)uc << ") to ("
                   << (int)c2 << "," << (int)uc2 << ")" << endl;

              // test int, unsigned int, long, unsigned long
              int i = wtto->get_ifield(wtto);
              unsigned int ui = wtto->get_uifield(wtto);
              long l = wtto->get_lfield(wtto);
              unsigned long ul = wtto->get_ulfield(wtto);
              int i2 = i + 1;
              unsigned int ui2 = ui + 1;
              long l2 = l + 1;
              unsigned long ul2 = ul + 1;
              wtto->set_ifield(i2, wtto);
              wtto->set_uifield(ui2, wtto);
              wtto->set_lfield(l2, wtto);
              wtto->set_ulfield(ul2, wtto);
              i2 = wtto->get_ifield(wtto);
              ui2 = wtto->get_uifield(wtto);
              l2 = wtto->get_lfield(wtto);
              ul2 = wtto->get_ulfield(wtto);
              cout << "(i,ui,l,ul) from ("
                   << i << "," << ui << "," << l << "," << ul
                   << ") to ("
                   << i2 << "," << ui2 << "," << l2 << "," << ul2
                   << ")" << endl;

              // test long long and unsigned long long
              long long ll = wtto->get_llfield(wtto);
              unsigned long long ull = wtto->get_ullfield(wtto);
              long long ll2 = ll+ 1;
              unsigned long long ull2 = ull+ 1;
              wtto->set_llfield(ll2, wtto);
              wtto->set_ullfield(ull2, wtto);
              ll2 = wtto->get_llfield(wtto);
              ull2 = wtto->get_ullfield(wtto);
              cout << "(ll,ull) from ("
                   << ll << "," << ull << ") to ("
                   << ll2 << "," << ull2 << ")" << endl;

              // test float and double
              float f = wtto->get_ffield(wtto);
              double d = wtto->get_dfield(wtto);
              float f2 = f + 1;
              double d2 = d + 1;
              wtto->set_ffield(f2, wtto);
              wtto->set_dfield(d2, wtto);
              f2 = wtto->get_ffield(wtto);
              d2 = wtto->get_dfield(wtto);
              cout << "(f,d) from ("
                   << f << "," << d << ") to ("
                   << f2 << "," << d2 << ")" << endl;
          } END_TRANSACTION;
      }

      // no data structure verification is implemented yet
      virtual bool verify(VerifyLevel_t v) { return true; }
  };
} // namespace bench

#endif // TYPETEST_HPP__
