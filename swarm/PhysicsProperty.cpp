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

#include "PhysicsProperty.hpp"

using namespace std;

void GamePhysicsProperty::ClearForces(wr_ptr<GamePhysicsProperty> wrThis)
{
    wrThis->set_afForceVector(0, 0, wrThis);
    wrThis->set_afForceVector(1, 0, wrThis);
    wrThis->set_afForceVector(2, 0, wrThis);
}

void GamePhysicsProperty::ClearForces(un_ptr<GamePhysicsProperty> unThis)
{
    unThis->set_afForceVector(0, 0, unThis);
    unThis->set_afForceVector(1, 0, unThis);
    unThis->set_afForceVector(3, 0, unThis);
}

void GamePhysicsProperty::AddForce(wr_ptr<GamePhysicsProperty> wrThis,
                                   float fNewX,
                                   float fNewY,
                                   float fNewZ)
{
    wrThis->set_afForceVector(0, wrThis->get_afForceVector(0, wrThis) + fNewX, wrThis);

    wrThis->set_afForceVector(1, wrThis->get_afForceVector(1, wrThis) + fNewY, wrThis);

    wrThis->set_afForceVector(2, wrThis->get_afForceVector(2, wrThis) + fNewZ, wrThis);
}

void GamePhysicsProperty::GetVelocity(rd_ptr<GamePhysicsProperty> rdThis,
                                      float& fNewX,
                                      float& fNewY,
                                      float& fNewZ) const
{
    fNewX = rdThis->get_afVelocityVector(0, rdThis);
    fNewY = rdThis->get_afVelocityVector(1, rdThis);
    fNewZ = rdThis->get_afVelocityVector(2, rdThis);
}

void GamePhysicsProperty::SetVelocity(wr_ptr<GamePhysicsProperty> wrThis,
                                      float fNewX,
                                      float fNewY,
                                      float fNewZ)
{
    wrThis->set_afVelocityVector(0, fNewX, wrThis);
    wrThis->set_afVelocityVector(1, fNewY, wrThis);
    wrThis->set_afVelocityVector(2, fNewZ, wrThis);
}

void GamePhysicsProperty::SetVelocity(un_ptr<GamePhysicsProperty> unThis,
                                      float fNewX,
                                      float fNewY,
                                      float fNewZ)
{
    unThis->set_afVelocityVector(0, fNewX, unThis);
    unThis->set_afVelocityVector(1, fNewY, unThis);
    unThis->set_afVelocityVector(2, fNewZ, unThis);
}

void GamePhysicsProperty::SetMass(wr_ptr<GamePhysicsProperty> wrThis,
                                  float fNewMass)
{
    wrThis->set_fMass(fNewMass, wrThis);
}

void GamePhysicsProperty::SetMass(un_ptr<GamePhysicsProperty> unThis,
                                  float fNewMass)
{
    unThis->set_fMass(fNewMass, unThis);
}


void GamePhysicsProperty::GetLookAtVectors(rd_ptr<GamePhysicsProperty> rdThis,
                                           Vector3 lookAt,
                                           Vector3 up,
                                           Vector3 right) const
{
    right[0] = rdThis->get_afForceVector(0, rdThis);
    right[1] = rdThis->get_afForceVector(1, rdThis);
    right[2] = rdThis->get_afForceVector(2, rdThis);

    rdThis->GetVelocity(rdThis, lookAt[0], lookAt[1], lookAt[2]);

    Vector3Normalize(lookAt);
    Vector3Normalize(right);

    Vector3Cross(up,lookAt,right);
    Vector3Normalize(up);

    Vector3Cross(right,up,lookAt);
    Vector3Normalize(right);
}

void GamePhysicsProperty::UpdateProperty(wr_ptr<GamePhysicsProperty> wrThis,
                                         wr_ptr<GameObject> wrMyObject,
                                         float fTimePassed,
                                         int iThreadID)
{

    float ax, ay, az; // future acceleration values
    float fx, fy, fz; // future force values
    float vx, vy, vz; // future velocity values
    float mass;

    fx = wrThis->get_afForceVector(0, wrThis);
    fy = wrThis->get_afForceVector(1, wrThis);
    fz = wrThis->get_afForceVector(2, wrThis);

    mass = wrThis->get_fMass(wrThis);

    // good old f = ma, lets find a = f/m
    ax = fx / mass;
    ay = fy / mass;
    az = fz / mass;

    vx = wrThis->get_afVelocityVector(0, wrThis);
    vy = wrThis->get_afVelocityVector(1, wrThis);
    vz = wrThis->get_afVelocityVector(2, wrThis);

    vx += ax * fTimePassed;
    vy += ay * fTimePassed;
    vz += az * fTimePassed;

    wrThis->set_afVelocityVector(0, vx, wrThis);
    wrThis->set_afVelocityVector(1, vy, wrThis);
    wrThis->set_afVelocityVector(2, vz, wrThis);


    sh_ptr<Spatial> spkSpatialProperty =
        wrMyObject->get_pkSpatialProperty(wrMyObject);

    if (spkSpatialProperty)
    {
        wr_ptr<Spatial> wrSpatialProperty =
            wr_ptr<Spatial>(spkSpatialProperty);

        wrSpatialProperty->MoveTranslation(wrSpatialProperty,
                                           vx * fTimePassed,
                                           vy * fTimePassed,
                                           vz * fTimePassed);
    }
}
