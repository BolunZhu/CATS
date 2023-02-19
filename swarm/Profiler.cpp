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

#include "Profiler.hpp"
#include <iostream>
using std::cout;
using std::endl;

// ------- Declare Extern(s) -------------------------------------------- //
Profiler PROF;

// ------- Profiler Control --------------------------------------------- //
bool Profiler::Init(unsigned long ulNumProfiles)
{
    m_akProfiles.resize(ulNumProfiles);
    return true;
}

// Set the time at which profiling started
void Profiler::StartProfiling(unsigned long long ullStartTimeStamp)
{
    // set start time here in order to compute total time
    m_ullBeginProfileTimeStamp = ullStartTimeStamp;
    // set start time for each thread so that first instantaneous average is
    // computed correctly
    for (unsigned long i = 0; i < GAMECONFIG.m_uiNumThreads; i++)
    {
        m_akProfiles[i].SetStartTime(ullStartTimeStamp);
    }
}

// End profiling and spit out a log.
void Profiler::EndProfiling(unsigned long long ullEndTimeStamp)
{
    double dSecPassed =
        getNumSecPassed(ullEndTimeStamp, m_ullBeginProfileTimeStamp);

    if (GAMECONFIG.m_bOutputCSV)
    {
        cout << "Inev Option, Time Stamp, "
             << "Command Line Args, "
             << "Run Time, "
             << "Frames, "
             << "Render Commits";

        for (unsigned long i = 0; i < GAMECONFIG.m_uiNumThreads; i++)
        {
            cout << ", "
                 << "Worker " << i << " Visual Objects, "
                 << "Worker " << i << " Fall Backs To Split, "
                 << "Worker " << i << " Fail Inevs, "
                 << "Worker " << i << " Update Skips Due To Time, "
                 << "Worker " << i << " Commits";
        }
        cout << endl;

        gPrintInevOption();
        cout << ", ";

        gPrintRunTimeStamp();
        cout << ", ";

        gPrintCmdLineArgs();
        cout << ", ";

        cout << (float)dSecPassed << ", "
             << GetEventCount(PROF_FRAME_RENDER,
                              GAMECONFIG.m_uiNumThreads) << ", "
             << GetEventCount(PROF_COMMIT_RENDER,
                              GAMECONFIG.m_uiNumThreads);

        for (unsigned long i = 0; i < GAMECONFIG.m_uiNumThreads; i++)
        {
            cout << ", "
                 << GetEventCount(PROF_VISUAL_OBJECT_UPDATE, i) << ", "
                 << GetEventCount(PROF_FELL_BACK_TO_SPLIT, i) << ", "
                 << GetEventCount(PROF_FAIL_INEV, i) << ", "
                 << GetEventCount(PROF_SKIP_NO_TIME_PASSED, i) << ", "
                 << GetEventCount(PROF_COMMIT_UPDATE, i);
        }
        cout << endl;
    }
    else
    {
        cout << endl
             << " ----- Profile Output --------------"
             << endl;

        cout << "Inev Option        :";
        gPrintInevOption();
        cout << endl;

        cout << "Time Stamp         :";
        gPrintRunTimeStamp();
        cout << endl;

        cout << "Command Line Args  :";
        gPrintCmdLineArgs();
        cout << endl;

        cout << "Run Time (sec)     :"
             << (float)dSecPassed << endl;

        cout << "Frames Rendered    :"
             << GetEventCount(PROF_FRAME_RENDER,
                              GAMECONFIG.m_uiNumThreads)
             << endl;

        cout << "Render Commits     :"
             << GetEventCount(PROF_COMMIT_RENDER,
                              GAMECONFIG.m_uiNumThreads)
             << endl;

        for (unsigned long i = 0; i < GAMECONFIG.m_uiNumThreads; i++)
        {
            cout << endl
                 << " ----- Thread "
                 << i
                 << " Report -------------" << endl;

            cout << "Visual Objects Updated :"
                 << GetEventCount(PROF_VISUAL_OBJECT_UPDATE, i)
                 << endl;

            cout << "Fall Backs To Split    :"
                 << GetEventCount(PROF_FELL_BACK_TO_SPLIT, i)
                 << endl;

            cout << "Fail Inevs             :"
                 << GetEventCount(PROF_FAIL_INEV, i)
                 << endl;

            cout << "Skipped VOs Due To Time:"
                 << GetEventCount(PROF_SKIP_NO_TIME_PASSED, i)
                 << endl;

            cout << "Commits                :"
                 << GetEventCount(PROF_COMMIT_UPDATE, i)
                 << endl;
        }
    }
}


// Print the log given the time stamp passed.
//
// [mfs] not currently in use
void Profiler::PrintIntermediateLog(unsigned long long ullTimeStamp) { }

// Log an event at thread level. Call each time id occurs.
void Profiler::LogEvent(EventType id, unsigned long uiThreadId)
{
    m_akProfiles[uiThreadId].m_aTotalCounts[id]++;
}

// Return the number of times event id has occurred in run of app.
unsigned long long
Profiler::GetEventCount(EventType id, unsigned long uiThreadId)
{
    return m_akProfiles[uiThreadId].m_aTotalCounts[id];
}

// Return the exponential moving average rate of event id's per second.
float Profiler::GetLocalEventRate(EventType id,
                                  unsigned long uiThreadId,
                                  unsigned long long ullTimeStamp)
{
    // time since last call
    double dSecondsPassed =
        getNumSecPassed(ullTimeStamp,
                        m_akProfiles[uiThreadId].m_aLastInstTime[id]);

    // avoid NAN
    if (dSecondsPassed < .00001)
    {
        return m_akProfiles[uiThreadId].m_afLastEMA[id];
    }

    // count total events that have occurred
    unsigned long long ullTotalEvents =
        m_akProfiles[uiThreadId].m_aTotalCounts[id];

    // events since last call
    unsigned long long ullDelta =
        ullTotalEvents - m_akProfiles[uiThreadId].m_aLastInstCount[id];

    // get change per unit time
    double dLocalAverage = ullDelta / dSecondsPassed;

    // save time, save most recent event count
    m_akProfiles[uiThreadId].m_aLastInstTime[id]  = ullTimeStamp;
    m_akProfiles[uiThreadId].m_aLastInstCount[id] = ullTotalEvents;

    // calculate the new EMA to smooth our rate
    double dCurrentEMA = m_akProfiles[uiThreadId].m_afLastEMA[id];
    dCurrentEMA += PROF_EMA_ALPHA * 
                  (dLocalAverage - m_akProfiles[uiThreadId].m_afLastEMA[id]);

    m_akProfiles[uiThreadId].m_afLastEMA[id] = (float)dCurrentEMA;

    return (float)dCurrentEMA;
}
