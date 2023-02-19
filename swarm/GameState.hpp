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
//  GameState.hpp -                                                       //
//    Main Controller of the Game. This is where the game logic is        //
//    updated, and the scene hierarchy is maintained.                     //
//                                                                        //
//    This is a singleton defined as GAMESTATE                            //
//                                                                        //
////////////////////////////////////////////////////////////////////////////
#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__

#include "Game.hpp"

class Spatial;
class GameObject;
class GameUpdateThread;
class thread;

class GameState
{

  public:
    GameState();
    ~GameState();

    // Called on startup, take care of creating threads
    bool Init(); 
    void Shutdown();

    // Wait until all updater threads have terminated.
    void JoinAll();

    // Call to update all objects. (For single thread mode)
    void Update();

    // List of GameObjects, each represents a visual object.
    // (Tripod or planet)
    sh_ptr<GameObject> * akGameObjects;

  private:

    // Update threads
    std::vector<GameUpdateThread*> akUpdaters;
    std::vector<thread*> akThreads;

};

// define singleton
extern GameState GAMESTATE;

#endif // __GAME_STATE_H__
