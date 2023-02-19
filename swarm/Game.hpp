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
//  Game.hpp -                                                            //
//    A game designed to use RSTM.                                        //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __GAME_HPP__
#define __GAME_HPP__

#include <time.h>
#include <vector>
#include <list>
#include <fstream>

// -- UNIX FIX
#ifndef _MSC_VER
#define sprintf_s sprintf
#endif

// -- STM INCLUDE ------------------------------------------------------- //
#include <config.h>
#include <stm/stm.hpp>
#include <stm/support/hrtime.h>
using namespace stm;

// -- COMMAND LINE ARGUMENTS -------------------------------------------- //
struct GameConfig
{
    unsigned int m_uiNumThreads;
    unsigned int m_uiNumObjects;
    unsigned int m_uiNumEmiterObjects;  // amount of "planet" like objects
    unsigned int m_uiWindowWidth;       // width of window
    unsigned int m_uiWindowHeight;      // height of window
    unsigned int m_uiRandomSeed;        // random number generator seed
    unsigned int m_uiTotalObjects;      // Total number of game objects to make
    unsigned int m_uiSpatialHashSize;   // Cube root of size of spatial hash
    unsigned int m_uiRenderBatchSize;   // visual objects drawn per render tx

    float m_fRunTime;                   // amount of time to run for
    float m_fHashGridWidth;             // spatial hashing grid width
    bool  m_bWindowedMode;              // windowed or not
    bool  m_bRenderBox;                 // Draw actual prisms for each object
    bool  m_bRenderGrad;
    bool  m_bUseAlpha;                  // For pseudo transparent objects
    bool  m_bUseAA;                     // Anti-Aliasing
    bool  m_bDrawBounds;
    bool  m_bOutputCSV;                 // Output results in CSV form.
    bool  m_bDrawHUD;                   // Show the Heads Up Display.
    bool  m_bVSyncLock;                 // Lock FPS to VSync.
    bool  m_bNoRenderInev;              // Do not try to become inev in the renderer
    bool  m_bNoUpdateInev;              // Do not try to become inev in the update threads

    GameConfig() :
        m_uiNumThreads(2),
        m_uiNumObjects(100),
        m_uiNumEmiterObjects(4),
        m_uiWindowWidth(1024),
        m_uiWindowHeight(768),
        m_uiRandomSeed(1),
        m_uiTotalObjects(m_uiNumEmiterObjects + m_uiNumObjects),
        m_uiSpatialHashSize(10),
        m_uiRenderBatchSize(10),
        m_fRunTime(-1.0f),
        m_fHashGridWidth(10.0f),
        m_bWindowedMode(true),
        m_bRenderBox(true),
        m_bRenderGrad(true),
        m_bUseAlpha(true),
        m_bUseAA(false),
        m_bDrawBounds(false),
        m_bOutputCSV(false),
        m_bDrawHUD(true),
        m_bVSyncLock(false),
        m_bNoRenderInev(false),
        m_bNoUpdateInev(true)
    { };
};

// -- Global Variables -------------------------------------------------- //
extern unsigned long long g_ullAppStartTime; // Start of app run.
extern unsigned long long g_ullTimerFreq;    // For windows timing code.
extern volatile bool      g_bIsAppAlive;     // False if game over.
extern GameConfig         GAMECONFIG;        // All game settings.

// -- Global Functions -------------------------------------------------- //
bool gHandleCommandLineArgs(int argc, char* argv[]);
void gKillGame();                // Kill the game and spit out logs.
bool gIsGameOver();              // True if game has been killed or
                                 // time is up.

void gPrintUsage();              // Print proper command line usage.
void gPrintInevOption();         // Print what type of inev app is using.
void gPrintRunTimeStamp();       // Print date and time of app run.
void gPrintCmdLineArgs();        // Print a string of app command line args.

double getNumSecPassed(unsigned long long end, unsigned long long start);

// -- GAME CLASSES ------------------------------------------------------ //
#include "Profiler.hpp"

// utilities
#include "STMList.hpp"
#include "MatrixMath.hpp"
#include "Collide.hpp"

// GameObject properties
#include "Spatial.hpp"
#include "ObjectBound.hpp"
#include "Material.hpp"
#include "Animation.hpp"
#include "GameObjectController.hpp"
#include "PhysicsProperty.hpp"

#include "GameObject.hpp"

// Game Singletons
#include "World.hpp"
#include "GameState.hpp"
#include "Renderer.hpp"

#endif // __GAME_H__
