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
uint32_t stat[4];//

/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** Initialize the counter */
#include <time.h>

 



 



// cout << "time passed is: " << (time2.tv_sec - time1.tv_sec)*1000 + (time2.tv_nsec - time1.tv_nsec)/1000000 << "ms" << endl;

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
void
bench_test(uintptr_t, uint32_t*)
{
        // struct timespec time1 = {0, 0};
        // struct timespec time2 = {0, 0};
        // struct timespec time3 = {0, 0};
        // struct timespec time4 = {0, 0};
        
    TM_BEGIN(atomic) {
// clock_gettime(CLOCK_REALTIME, &time4);
        // increment the counter
        uint64_t tmp=TM_READ(counter);
        TM_WRITE(counter, 1 +tmp );
        // clock_gettime(CLOCK_REALTIME, &time1);
        for (uint64_t i = 0; i < COM_LEN; i++)
        {
            tmp=f(tmp);
            // if(i%100==0){printf("!");}
            
        }
        // clock_gettime(CLOCK_REALTIME, &time2);
        // printf("time2.tv_sec - time1.tv_sec=%d \n time2.tv_nsec - time1.tv_nsec=%d\n",time2.tv_sec - time1.tv_sec,time2.tv_nsec - time1.tv_nsec);
        TM_WRITE(a[(tmp-COM_LEN)%MAX_NUM],tmp); 
        

    } TM_END;
    // clock_gettime(CLOCK_REALTIME, &time3);
        // stat[0]=time2.tv_nsec - time1.tv_nsec;
        // stat[1]=time3.tv_nsec - time4.tv_nsec;

    // printf("time3.tv_sec - time4.tv_sec=%d \n time3.tv_nsec - time4.tv_nsec=%d\n",time3.tv_sec - time4.tv_sec,time3.tv_nsec - time4.tv_nsec);
}

/*** Ensure the final state of the benchmark satisfies all invariants */
bool
bench_verify()
{
    std::cout << "(final value = " << counter << ") ";
    // printf("time2.tv_nsec - time1.tv_nsec=%d\n time3.tv_nsec - time4.tv_nsec=%d\n",stat[0],stat[1]);
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
