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

#ifndef COUNTER_HPP__
#define COUNTER_HPP__

#include <stm/stm.hpp>
#include "Benchmark.hpp"
#include <iostream>

// the type being counted
#define MYTYPE int

namespace bench
{
    class Counter : public stm::Object
    {
        GENERATE_FIELD(MYTYPE, value);

      public:
        Counter(MYTYPE startingValue = 0) : m_value(startingValue) { }
    };



    class CounterBench : public Benchmark
    {
      private:

        stm::sh_ptr<Counter> m_counter;

      public:

        CounterBench() : m_counter(new Counter())
        { }


        void random_transaction(thread_args_t* args, unsigned int* seed,
                                unsigned int val,    int chance)
        {
            BEGIN_TRANSACTION;
            stm::wr_ptr<Counter> wr(m_counter);
            wr->set_value(wr->get_value(wr) + 1, wr);
            END_TRANSACTION;
        }


        bool sanity_check() const
        {
            // not as useful as it could be...
            MYTYPE val = 0;
            BEGIN_TRANSACTION;
            stm::rd_ptr<Counter> rd(m_counter);
            val = rd->get_value(rd);
            END_TRANSACTION;
            std::cout << "final value = " << val << std::endl;
            return (val > 0);
        }


        // no data structure verification is implemented for the Counter...
        // yet
        virtual bool verify(VerifyLevel_t v) {
            return true;
        }
    };

}   // namespace bench

#endif  // COUNTER_HPP__
