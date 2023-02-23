/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <stm/config.h>

#if defined(STM_CPU_SPARC)
#include <sys/types.h>
#endif

#include <stdint.h>
#include <iostream>
#include <api/api.hpp>
#include "bmconfig.hpp"

/**
 *  We provide the option to build the entire benchmark in a single
 *  source. The bmconfig.hpp include defines all of the important functions
 *  that are implemented in this file, and bmharness.cpp defines the
 *  execution infrastructure.
 */
#ifdef SINGLE_SOURCE_BUILD
#include "bmharness.cpp"
#endif

/**
 *  Step 1:
 *    Include the configuration code for the harness, and the API code.
 */

/**
 *  Step 2:
 *    Declare the data type that will be stress tested via this benchmark.
 *    Also provide any functions that will be needed to manipulate the data
 *    type.  Take care to avoid unnecessary indirection.
 *
 *  NB: For the simple counter, we don't need to have an abstract data type
 */

/*** the counter we will manipulate in the experiment */
const uint64_t MAX_NUM=1024;
const uint64_t OP_LEN = 16;
uint64_t COM_LEN = 1000;
uint64_t counter;
uint64_t a[MAX_NUM];

/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** Initialize the counter */
void
bench_init()
{
    counter = 0;
    COM_LEN = CFG.ops;
}
uint64_t f(uint64_t t){ 
    uint64_t res=t*t+2*t+1;
    res=res/(t+1);
    return res;
}
/*** Run a bunch of increment transactions */
using stm::TxThread;
void fn(TxThread * tx, uint32_t input_index,uint32_t output_index){
    uint64_t addr_tmp= TM_GET_INPUT(input_index);
    uint64_t addr_a  = TM_GET_OUTPUT(output_index);
    for (uint64_t i = 0; i < COM_LEN; i++)
    {
        (*reinterpret_cast<uint64_t *>(addr_tmp))=f((*reinterpret_cast<uint64_t *>(addr_tmp)));
        // if(i%100==0){printf("!");}
    }
    TM_WRITE_p(reinterpret_cast<uint64_t *>(addr_a),TM_READ_p(reinterpret_cast<uint64_t *>(addr_tmp)));
}
void
bench_test(uintptr_t, uint32_t*)
{
    TM_BEGIN(atomic) {
        // increment the counter
        uint64_t tmp=TM_READ(counter);
        TM_WRITE(counter, 1 +tmp );
        TM_DELAY(
            TM_INPUT(reinterpret_cast<uint64_t>(&tmp)),
            TM_OUTPUT(reinterpret_cast<uint64_t>(&(a[(tmp-COM_LEN)%MAX_NUM]))), 
            reinterpret_cast<uint64_t>(fn)
        );
    } TM_END;
}

/*** Ensure the final state of the benchmark satisfies all invariants */
bool
bench_verify()
{
    std::cout << "(final value = " << counter << ") ";
    return (counter > 0);
}

/**
 *  Step 4:
 *    Include the code that has the main() function, and the code for creating
 *    threads and calling the three above-named functions.  Don't forget to
 *    provide an arg reparser.
 */

/*** no reparsing needed */
void
bench_reparse() {
    CFG.bmname = "Test1";
}
