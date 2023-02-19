/******************************************************************************
 *
 * Copyright (c) 2007, 2008, 2009
 * University of Rochester
 * Department of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the name of the University of Rochester nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


////////////////////////////////////////////////////////////////////////////
//  PROFILER.hpp -                                                        //
//    Record performance information about game.                          //
//                                                                        //
//    In general we store counts of events enumerated by EventType to     //
//    avoid calls to getElapsedTime. We also can calculate the local      //
//    average rate of a given event by passing time stamps to the         //
//    profiler. If there are no calls to the local average rate           //
//    functions, there is only space overhead.                            //
//                                                                        //
//    Profiler is a singleton: one for entire app. Access using "PROF."   //
//                                                                        //
////////////////////////////////////////////////////////////////////////////


#ifndef __PROFILER_H__
#define __PROFILER_H__

#include "Game.hpp"

// Smoothing factor for Exponential Moving Averages
#define PROF_EMA_ALPHA .075f

class Profiler
{
  public:

    // -- EventType ------------------------------------------------- //
    //  Enum identifying the different types of events that the 
    //  Profiler counts.
    enum EventType
    {
        PROF_FRAME_RENDER,         // Entire frame rendered: GLpresent.
        PROF_COMMIT_RENDER,        // Render thread committed txn.
        PROF_VISUAL_OBJECT_UPDATE, // Visual obj. (Tripods, etc.) updated.
        PROF_BATCH_UPDATE,         // Thread's alloted objects are updated.
        PROF_VISUAL_OBJECT_SKIP,   // Visual obj. skipped till next pass.
        PROF_COMMIT_UPDATE,        // Commit in one of the worker threads.
        PROF_FAIL_INEV,            // A txn failed to become inev.
        PROF_SKIP_NO_TIME_PASSED,  // Updater stopped b/c no time passed.
        PROF_FELL_BACK_TO_SPLIT,   // A big update txn failed, fall back
                                   // to split transactions.
        NUM_PROF                   // Mark size of enum
    };

  private:
    // -- EventHistogram -------------------------------------------- //
    //  Counter for each EventType, with support for total count and
    //  instantaneous averages.
    struct EventHistogram
    {
        // Count total number of event occurrences
        unsigned long long m_aTotalCounts[NUM_PROF];

        // When we want an instantaneous average, we compute differences
        // between the total event count and the event count at last
        // instantaneous average, and divide by the time between now and the
        // time the last instantaneous average was taken.

        //  NB: If we don't call the instantaneous average functions, there is
        //  no time overhead.  Even when we do call those functions, there
        //  should be no calls to getTime in this code.

        // The TotalCount when we last did an instantaneous average
        unsigned long long m_aLastInstCount[NUM_PROF];

        // Time when LastInstCount was last updated
        unsigned long long m_aLastInstTime[NUM_PROF];

        // Store an Exponential Moving Average to smooth the wildly varying
        // instantaneous rates.
        float m_afLastEMA[NUM_PROF];

        // Constructor: zero the counts.
        EventHistogram()
        {
            for (int i = 0; i < NUM_PROF; i++)
            {
               m_aTotalCounts[i] = 0;
               m_aLastInstCount[i] = 0;
               m_aLastInstTime[i] = 0;

               m_afLastEMA[i] = 0;
            }
        };

        // Reset the LastInstTime fields based on a start time.
        void SetStartTime(unsigned long long t)
        {
            for (int i = 0; i < NUM_PROF; i++) {
                m_aLastInstTime[i] = t;
            }
        }

    };

    // Vector of per-thread info. If EventHistogram is not cache-line
    // padded we can have unexpected misses.
    std::vector<EventHistogram> m_akProfiles;

    // Time stamp for start of profiling. This may not be the exact start
    // of program execution.
    unsigned long long m_ullBeginProfileTimeStamp;

  public:
    Profiler() { };

    // ------- Profiler Control ----------------------------------------- //
    // Pass the number of profiles to create. We use an extra profile
    // for the render thread, and one for each updater.
    bool Init(unsigned long ulNumProfiles);

    // Take a time stamp and init values.
    void StartProfiling(unsigned long long ullStartTimeStamp);

    // End profiling and spit out a log.
    void EndProfiling(unsigned long long ullEndTimeStamp);

    // ------- Printing Profile Information ----------------------------- //
    // Print the log given the time stamp passed.
    void PrintIntermediateLog(unsigned long long ullTimeStamp);


    // ------- Logging Profile Information ------------------------------ //
    // Log an event at thread level. Call each time id occurs.
    void LogEvent(EventType id, unsigned long uiThreadId);


    // ------- Getting Profile Information ------------------------------ //

    // Return the number of times event id has occurred in run of app.
    unsigned long long GetEventCount(EventType id, unsigned long uiThreadId);

    // Return the local average rate of event id's per second.
    float GetLocalEventRate(EventType id,
                            unsigned long uiThreadId,
                            unsigned long long ullTimeStamp);
};

extern Profiler PROF;

#endif // __PROFILER_H__
