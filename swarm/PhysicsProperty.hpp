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
//  PhysicsProperty.hpp -                                                 //
//    Property to govern physics information. Includes forces and mass    //
//    and friction.                                                       //
//                                                                        //
//    Apply this property to the root object of something you want to be  // 
//    affected by physics.                                                //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __GAMEPHYSICSPROPERTY_H__
#define __GAMEPHYSICSPROPERTY_H__

#include "Game.hpp"

class GamePhysicsProperty : public Object {

    // -- RSTM API Generated fields --------------------------------- //
    // current forces acting on the object
    GENERATE_ARRAY(float, afForceVector,    3);
    GENERATE_ARRAY(float, afVelocityVector, 3);
    GENERATE_FIELD(float, fMass);

  public:

    GamePhysicsProperty() : m_fMass(1.0f)
    { }

    void ClearForces(wr_ptr<GamePhysicsProperty> wrThis);
    void ClearForces(un_ptr<GamePhysicsProperty> unThis);

    void AddForce(wr_ptr<GamePhysicsProperty> wrThis,
                  float fNewX, float fNewY, float fNewZ);

    void GetVelocity(rd_ptr<GamePhysicsProperty> rdThis,
                     float& fNewX, float& fNewY, float& fNewZ) const;

    void SetVelocity(wr_ptr<GamePhysicsProperty> wrThis,
                     float fNewX, float fNewY, float fNewZ);
    void SetVelocity(un_ptr<GamePhysicsProperty> unThis,
                     float fNewX, float fNewY, float fNewZ);

    void SetMass(wr_ptr<GamePhysicsProperty> wrThis,
                 float fNewMass);
    void SetMass(un_ptr<GamePhysicsProperty> unThis,
                 float fNewMass);

    // Figure out where to point based on physics forces.
    void GetLookAtVectors(rd_ptr<GamePhysicsProperty> rdThis,
                          Vector3 lookAt,
                          Vector3 up,
                          Vector3 right) const;

    // Move the object based on current acceleration and velocity.
    void UpdateProperty(wr_ptr<GamePhysicsProperty> wrThis,
                        wr_ptr<GameObject> wrMyObject,
                        float fTimePassed,
                        int iThreadID = -1);
};

#endif // __GAMEPHYSICSPROPERTY_H__
