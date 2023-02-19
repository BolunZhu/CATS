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

#include "Game.hpp"

#ifdef _MSC_VER
#include "../alt-license/XGetOpt.h"
#endif

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <string>
using std::string;

// -- Extern Declarations ----------------------------------------------- //
volatile bool       g_bIsAppAlive = true;
unsigned long long  g_ullAppStartTime = 0;
unsigned long long  g_ullTimerFreq = 1000000000;
GameConfig          GAMECONFIG;
string              g_strCmdArgs = "";

// Print msg explaining that app must end.
void gFailInit(const char* const c_str);

// -- main -------------------------------------------------------------- //
int main(int argc, char* argv[])
{
    // copy argv array before passing it to gHandleCommandLineArgs
    char** argv2 = new char*[argc];
    for (int i = 0; i < argc; i++) {
        string tmp(argv[i]);
        argv2[i] = new char[tmp.length()];
        strcpy(argv2[i], tmp.c_str());
    }

    // -- Command Line ---------------------------------------------- //
    if (gHandleCommandLineArgs(argc, argv2) == false)
        return (0);

    // -- Up Clock Speed -------------------------------------------- //
    //  This is a hack to turn up the clock speed for windows to
    //  ensure quick context switches.
#ifdef _MSC_VER

    QueryPerformanceFrequency((LARGE_INTEGER*)&g_ullTimerFreq);

    TIMECAPS tc;
    UINT     wTimerRes;

    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
    {
        // We failed, continue anyway.
    }

    wTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
    timeBeginPeriod(wTimerRes);
#endif


    // -- Seed Rand ------------------------------------------------- //
    srand(GAMECONFIG.m_uiRandomSeed);

    // -- Start the Clock ------------------------------------------- //
    g_ullAppStartTime = RENDER.GrabTimeStamp();

    // -- Init RSTM ------------------------------------------------- //
    stm::init("Polka", "invis-eager", true);

    //  Init all game singletons
    PROF.Init(GAMECONFIG.m_uiNumThreads + 1);
    RENDER.Init(argc, argv);
    WORLD.Init();
    GAMESTATE.Init();

    // -- Wait For Game Termination --------------------------------- //
    //  The game is running now, wait for it to terminate here.
    GAMESTATE.JoinAll();

    // wait for the renderer to shut down
    while (!RENDER.isRendererShutdown()) { sleep_ms(10); }

    // -- Shutdown -------------------------------------------------- //
    //  Clean up, print logs.
    PROF.EndProfiling(RENDER.GrabTimeStamp());
    RENDER.Shutdown();
    GAMESTATE.Shutdown();

    return(0);
}

// -- gHandleCommandLineArgs --------------------------------------------- //
//  Command Line Argument Handling (Loop and look for args that matter)
bool gHandleCommandLineArgs(int argc, char* argv[])
{
    int opt;
    string tmpstr = "";
    bool bProblem = false;

    // parse the command-line options
    while ((opt = getopt(argc, argv, "p:t:s:A:G:R:I:D:W:H:FZ:Y:O:h")) != -1) {
        switch(opt) {
            // Benchmark Configuration
          case 'p':
            GAMECONFIG.m_uiNumThreads = atoi(optarg);
            break;
          case 't':
            GAMECONFIG.m_fRunTime = atof(optarg);
            break;
          case 's':
            GAMECONFIG.m_uiRandomSeed = atoi(optarg);
            break;
            // Game Object Configuration
          case 'A':
            GAMECONFIG.m_uiNumObjects = atoi(optarg);
            break;
          case 'G':
            GAMECONFIG.m_uiNumEmiterObjects = atoi(optarg);
            break;
            // Rendering Configuration
          case 'R':
            GAMECONFIG.m_uiRenderBatchSize = atoi(optarg);
            break;
          case 'I':
            tmpstr = string(optarg);
            if (tmpstr == "none") {
                GAMECONFIG.m_bNoRenderInev = true;
                GAMECONFIG.m_bNoUpdateInev = true;
            }
            else if (tmpstr == "render") {
                GAMECONFIG.m_bNoRenderInev = false;
                GAMECONFIG.m_bNoUpdateInev = true;
            }
            else if (tmpstr == "update") {
                GAMECONFIG.m_bNoRenderInev = true;
                GAMECONFIG.m_bNoUpdateInev = false;
            }
            else {
                printf("Unrecognized option to -I: %s\n", optarg);
                bProblem = true;
            }
            break;
          case 'D': // Drawing Options
            tmpstr = string(optarg);
            if      (tmpstr == "novsync")  { GAMECONFIG.m_bVSyncLock = false; }
            else if (tmpstr == "vsync")    { GAMECONFIG.m_bVSyncLock = true; }
            else if (tmpstr == "nobounds") { GAMECONFIG.m_bDrawBounds = false; }
            else if (tmpstr == "bounds")   { GAMECONFIG.m_bDrawBounds = true; }
            else if (tmpstr == "noalpha")  { GAMECONFIG.m_bUseAlpha = false; }
            else if (tmpstr == "alpha")    { GAMECONFIG.m_bUseAlpha = true; }
            else if (tmpstr == "noAA")     { GAMECONFIG.m_bUseAA = false; }
            else if (tmpstr == "AA")       { GAMECONFIG.m_bUseAA = true; }
            else if (tmpstr == "noline")   { GAMECONFIG.m_bRenderBox = true; }
            else if (tmpstr == "line")     { GAMECONFIG.m_bRenderBox = false; }
            else if (tmpstr == "nograd")   { GAMECONFIG.m_bRenderGrad = false; }
            else if (tmpstr == "grad")     { GAMECONFIG.m_bRenderGrad = true; }
            else {
                printf("Unrecognized option to -D: %s\n", optarg);
                bProblem = true;
            }
            break;
            // Screen Options
          case 'W':
            GAMECONFIG.m_uiWindowWidth = atoi(optarg);
            break;
          case 'H':
            GAMECONFIG.m_uiWindowHeight = atoi(optarg);
            break;
          case 'F':
            GAMECONFIG.m_bWindowedMode = false;
            break;
            // Spatial Configuration
          case 'Z':
            GAMECONFIG.m_uiSpatialHashSize = atoi(optarg);
            break;
          case 'Y':
            GAMECONFIG.m_fHashGridWidth = atof(optarg);
            break;
            // Output Options
          case 'O':
            tmpstr = string(optarg);
            if      (tmpstr == "nohud") { GAMECONFIG.m_bDrawHUD = false; }
            else if (tmpstr == "hud")   { GAMECONFIG.m_bDrawHUD = true; }
            else if (tmpstr == "nocsv") { GAMECONFIG.m_bOutputCSV = false; }
            else if (tmpstr == "csv")   { GAMECONFIG.m_bOutputCSV = true; }
            break;
          case 'h':
            bProblem = true;
            break;
          default:
            printf("Unrecognized option -%c\n", opt);
            bProblem = true;
            break;
        }
    }

    for (int i = 1; i < argc; i++) {
        g_strCmdArgs.append(argv[i]);
        g_strCmdArgs.append(" ");
    }

    GAMECONFIG.m_uiTotalObjects =
        GAMECONFIG.m_uiNumEmiterObjects + GAMECONFIG.m_uiNumObjects;

    if (bProblem) {
        gPrintUsage();
        return false;
    }

    return true;
}

// -- gKillGame --------------------------------------------------------- //
//  Call to stop the game and spit out whatever profiling information
//  may be needed.
//
//  The function itself simply resets g_bIsAppAlive, each thread
//  must then react accordingly.
void gKillGame()
{
    g_bIsAppAlive = false;
}

// -- gIsGameOver ------------------------------------------------------- //
//  Returns true if g_bIsAppAlive is false, or if the app run time has
//  passed.
//
//  Each update thread checks this function after updating its objects
//  to ensure the threads terminate correctly.
bool gIsGameOver()
{
    if (!g_bIsAppAlive)
    {
        return true;
    }

    // m_fRunTime is negative if there is no time limit.
    if (GAMECONFIG.m_fRunTime < 0)
    {
        return false;
    }

    float fSecRan = getNumSecPassed(RENDER.GrabTimeStamp(), g_ullAppStartTime);
    if (GAMECONFIG.m_fRunTime < fSecRan)
    {
        return true;
    }

    return false;
}

// -- gPrintUsage ------------------------------------------------------- //
//  Called to help user. Must be modified if args are changed.
void gPrintUsage()
{
    cout << "Command Line Option Summary" << endl;
    cout << "Benchmark Configuration" << endl;
    cout << "  p[i]   number of updater threads   (default = 2)" << endl;
    cout << "  t[i]   run benchmark for i seconds (default = infinite)" << endl;
    cout << "  s[i]   random number seed          (default = 1)" << endl;
    cout << endl;
    cout << "Game Object Configuration" << endl;
    cout << "  A[i]   number of AMOs   (default = 100)" << endl;
    cout << "  G[i]   number of GEOs   (default = 4)" << endl;
    cout << endl;
    cout << "Rendering Configuration" << endl;
    cout << "  R[i]   AMOs to render per transaction (default 10)" << endl;
    cout << "  I[s]   Inevitability" << endl;
    cout << "     none   -- no transactions use inevitability" << endl;
    cout << "     render -- renderer uses inevitability (default)" << endl;
    cout << "     update -- AMO updaters try to use inevitability" << endl;
    cout << "  D[s]   Drawing options (can be prefixed with no)" << endl;
    cout << "     vsync  -- lock render rate to screen refresh  (default off)"
         << endl;
    cout << "     bounds -- show collision boundaries           (default off)"
         << endl;
    cout << "     alpha  -- allow render transparency           (default on)"
         << endl;
    cout << "     AA     -- anti-aliasing                       (default off)"
         << endl;
    cout << "     line   -- draw AMOs as lines                  (default off)"
         << endl;
    cout << "     grad   -- draw background gradient            (default on)"
         << endl;
    cout << endl;
    cout << "Screen Options" << endl;
    cout << "  W[i]   Screen Width     (default 1024)" << endl;
    cout << "  H[i]   Screen Height    (default 768)" << endl;
    cout << "  F      Full-screen mode" << endl;
    cout << endl;
    cout << "Spatial Configuration" << endl;
    cout << "  Z[i]  use i^3 buckets for the spatial hash (default = 19)" << endl;
    cout << "  Y[f]  width of spatial hash buckets        (default = 10.0)"
         << endl;
    cout << endl;
    cout << "Output Options" << endl;
    cout << "  O[s]   Text Output (can be prefixed with no)" << endl;
    cout << "     hud  -- show heads-up display     (default on)" << endl;
    cout << "     csv  -- print statistics as a csv (default off)" << endl;
    cout << "  h      Print help (this message)" << endl;
}

// -- gPrintInevOption -------------------------------------------------- //
//  Use ifdefs to check what the inev option is. This is a redundancy to
//  prevent accidentally building with the wrong option defined.
void gPrintInevOption()
{
#if defined(STM_INEV_GRL)
    printf("STM_INEV_GRL");
#elif defined(STM_INEV_GWL)
    printf("STM_INEV_GWL");
#elif defined(STM_INEV_GWLFENCE)
    printf("STM_INEV_GWLFENCE");
#elif defined(STM_INEV_DRAIN)
    printf("STM_INEV_DRAIN");
#elif defined(STM_INEV_BLOOM_SMALL)
    printf("STM_INEV_BLOOM_SMALL");
#elif defined(STM_INEV_BLOOM_MEDIUM)
    printf("STM_INEV_BLOOM_MEDIUM");
#elif defined(STM_INEV_BLOOM_LARGE)
    printf("STM_INEV_BLOOM_LARGE");
#elif defined(STM_INEV_IRL)
    printf("STM_INEV_IRL");
#elif defined(STM_INEV_NONE)
    printf("STM_INEV_NONE");
#endif
}

// -- gPrintRunTimeStamp ------------------------------------------------ //
//  Generate a time stamp that contains a date to help identify runs.
void gPrintRunTimeStamp()
{
    time_t ltime = time(NULL); // get current cal time

    // asctime adds a return character, this code strips it off.
    // copy our time stamp into buff
    char buff[200];
    sprintf(buff,"%s",asctime(localtime(&ltime)));
    buff[strlen(buff)-1] = '\0';

    // print the time stamp without a return character so it can be used in a
    // CSV output.
    printf(buff);
}

// -- gPrintCmdLineArgs ------------------------------------------------- //
//  Print the saved command args to help identify runs.
void gPrintCmdLineArgs()
{
    printf(g_strCmdArgs.c_str());
}

// -- gFailInit --------------------------------------------------------- //
//  Print msg explaining that app must end.
void gFailInit(const char* const c_str)
{
    printf("Failed to init the %s. Terminating.\n", c_str);
}

// -- getNumSecPassed --------------------------------------------------- //
//  Different frequency for OS. I assume these values are good for all
//  machines of a given OS.
double getNumSecPassed(unsigned long long end, unsigned long long start)
{
    unsigned long long passed = end - start;
    return(((double)passed / g_ullTimerFreq));
}
