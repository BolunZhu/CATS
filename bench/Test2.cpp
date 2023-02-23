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
uint64_t OP_LEN = 16;
uint64_t COM_LEN = 1000;
uint64_t counter;
uint64_t a[MAX_NUM];
uint32_t stat[4];//
uint64_t POS=0;
/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** Initialize the counter */
#include <time.h>

void
bench_init()
{
    counter = 0;
    COM_LEN = CFG.ops;  // -O = computation cost
    OP_LEN = CFG.elements; // -m = # of operations
    POS=(CFG.lookpct*CFG.elements)/100; // -R = insert hotspot at x% of tx. 0=head 100=tail 
}
uint64_t f(uint64_t t){ 
    uint64_t res=t*t+2*t+1;
    res=res/(t+1);
    return res;
}
/*** Run a bunch of increment transactions */
void
bench_test(uintptr_t, uint32_t* seed)
{
    volatile uint32_t local_seed = *seed;
    TM_BEGIN(atomic) {
        uint64_t v = rand_r_32(&local_seed);
        uint64_t k = rand_r_32(&local_seed);
        for(uint64_t i = 0; i< OP_LEN; i++){
            if(i==POS){
                uint64_t tmp=TM_READ(counter);
                TM_WRITE(counter, 1 +tmp );
            }
            for(uint64_t j=0; j<COM_LEN; j++){
                v=f(v);
            }
            TM_WRITE(a[(k+i)%MAX_NUM],v);
        }
        if(POS==OP_LEN){
            uint64_t tmp=TM_READ(counter);
            TM_WRITE(counter, 1 +tmp );
        }
    } TM_END;
    *seed = local_seed;
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
    CFG.bmname = "Test2";
}
