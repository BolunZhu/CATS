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
//  GameObjectController.hpp                                                //
//    Basic state machine to control AI for game                          //
//    objects. These will change the state of                             //
//    other objects as well.                                              //
//                                                                        //
//    For general game programming, most of the                           //
//    state transition functions would need to                            //
//    be changed to whatever the game needs.                              //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __GAMEOBJECTCONTROLLER_H__
#define __GAMEOBJECTCONTROLLER_H__

#include "Game.hpp"

class Spatial;

class GameObjectController : public Object {

    // -- RSTM API Generated fields -----------//
    GENERATE_FIELD(int,   iState);     // my state machine state
    GENERATE_FIELD(float, fHealth);    // how much abstract 'health' I have
    GENERATE_FIELD(int,   iAnimation); // what animation I'm trying to do
    GENERATE_FIELD(unsigned long long, ullLastShootTS); // shoot time stamp
    GENERATE_FIELD(sh_ptr<GameObject>, pkTarget); // to seek and destroy!

  public:
    // Game Object Controller States
    enum { GOCS_FLOAT_PHYS, GOCS_FLOAT, GOCS_HELD, GOCS_DEATH };

    // Game Object Controller Messages
    enum { GOCM_SET_TARGET, GOCM_COLLIDED };

    void UpdateState(wr_ptr<GameObjectController> wrThis,
                     wr_ptr<GameObject> wrMyObject,
                     unsigned long long ullTimeStamp,
                     int iThreadID = -1);

    void SetState(wr_ptr<GameObjectController> wrThis, int iNewState);
    void SetState(un_ptr<GameObjectController> unThis, int iNewState);

    void OnMsg(wr_ptr<GameObjectController> wrThis, int iMsg, void* msgData,
               sh_ptr<GameObject> rdMyObject = sh_ptr<GameObject>(NULL));
    // call this when you want to take health
    // or some such thing

  private:
    void CalculateGravitationalForce(float mass,
                                     float x, float y, float z,
                                     float& fx, float& fy, float& fz);
    // Return the value of the force vector due to gravitational pull
    // useless in a "general" game
};

#endif // __GAMEOBJECTCONTROLLER_H__
