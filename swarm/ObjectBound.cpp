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

#include "ObjectBound.hpp"

using namespace std;

void GameObjectBound::ExpandBound(wr_ptr<GameObjectBound> wrThis,
                                  rd_ptr<GameObjectBound> rdOther)
{
    float myX, myY, myZ, myRadius, otherX, otherY, otherZ, otherRadius;

    wrThis->GetBound(wrThis,myX,myY,myZ,myRadius);
    rdOther->GetBound(rdOther,otherX,otherY,otherZ,otherRadius);

    Vector3 v3Me = {myX, myY, myZ};
    Vector3 v3Across = {otherX - myX, otherY- myY, otherZ - myZ};
    Vector3 v3NormalAcross = {otherX - myX, otherY- myY, otherZ - myZ};

    float d = Vector3Length(v3Across);

    Vector3Normalize(v3NormalAcross);

    if (d + otherRadius < myRadius || d + myRadius < otherRadius)
    {
        return; // we're good
    }

    Vector3 v3Left;
    Vector3 v3Right;
    Vector3 v3TotalD;
    for (int i = 0; i < 3; i++)
    {
        v3Left[i]   = v3NormalAcross[i] * -myRadius;
        v3Right[i]  = v3NormalAcross[i] * (d+otherRadius);
        v3TotalD[i] = v3Right[i] - v3Left[i];
        wrThis->set_afBoundCenter(i,v3Me[i] + (v3Right[i] + v3Left[i])/2.0f, wrThis);
    }

    float newRadius = Vector3Length(v3TotalD)/2.0f;
    wrThis->set_fBoundRadius(newRadius, wrThis);
}

bool GameObjectBound::ChangedBox(stm::rd_ptr<GameObjectBound> rdThis) const
{
    return (WORLD.GetBox(rdThis) != rdThis->get_iMyBox(rdThis));
}

bool GameObjectBound::LineIsInBound(rd_ptr<GameObjectBound> rdThis,
                                    float startX, float startY, float startZ,
                                    float endX, float endY, float endZ) const
{
    Vector3 vStart = {startX,startY,startZ};
    Vector3 vLine = { -startX+endX, -startY+endY, -startZ+endZ };
    Vector3 vOther, vRes;

    for (int i = 0; i < 3; i++)
    {
        vOther[i] = vStart[i] - rdThis->get_afBoundCenter(i, rdThis);
    }


    Vector3Cross(vRes,vLine,vOther);

    float d = Vector3Length(vRes)/Vector3Length(vLine);

    return (d <rdThis->get_fBoundRadius(rdThis));
}

bool GameObjectBound::IsInBound(rd_ptr<GameObjectBound> rdThis,
                                const float& x,
                                const float& y,
                                const float& z) const
{
    Vector3 pos, otherPos;

    otherPos[0] = x;
    otherPos[1] = y;
    otherPos[2] = z;

    for (int i = 0; i < 3; i++)
    {
        pos[i] =  rdThis->get_afBoundCenter(i, rdThis) - otherPos[i];
    }

    return Vector3Length(pos) < rdThis->get_fBoundRadius(rdThis);
}

bool GameObjectBound::IntersectsBound(rd_ptr<GameObjectBound> rdThis,
                                      float radius,
                                      float x,
                                      float y,
                                      float z) const
{
    Vector3 pos, otherPos;

    otherPos[0] = x;
    otherPos[1] = y;
    otherPos[2] = z;

    for (int i = 0; i < 3; i++)
    {
        pos[i] =  rdThis->get_afBoundCenter(i, rdThis) - otherPos[i];
    }

    return Vector3Length(pos) < rdThis->get_fBoundRadius(rdThis) + radius;
}

void GameObjectBound::SetBound(un_ptr<GameObjectBound> unThis,
                               float radius, float x, float y, float z)
{
    unThis->set_afLocalBoundCenter(0, x, unThis);
    unThis->set_afLocalBoundCenter(1, y, unThis);
    unThis->set_afLocalBoundCenter(2, z, unThis);
    unThis->set_fLocalBoundRadius(radius, unThis);
}

void GameObjectBound::GetBound(rd_ptr<GameObjectBound> rdThis,
                               float& x,
                               float& y,
                               float& z,
                               float& radius) const
{
    x = rdThis->get_afBoundCenter(0, rdThis);
    y = rdThis->get_afBoundCenter(1, rdThis);
    z = rdThis->get_afBoundCenter(2, rdThis);

    radius = rdThis->get_fBoundRadius(rdThis);
}

void GameObjectBound::UpdateBound(wr_ptr<GameObjectBound> wrThis,
                                  rd_ptr<Spatial> rdMyObject,
                                  int iThreadID)
{

    float x, y, z, radius;
    float x2, y2, z2;
    x2 = x = wrThis->get_afLocalBoundCenter(0, wrThis);
    y2 = y = wrThis->get_afLocalBoundCenter(1, wrThis);
    z2 = z = wrThis->get_afLocalBoundCenter(2, wrThis);

    radius = wrThis->get_fLocalBoundRadius(wrThis);
    x2 += radius;

    rdMyObject->TransformPoint(rdMyObject, x, y, z);
    rdMyObject->TransformPoint(rdMyObject, x2, y2, z2);
    Vector3 dist = {x-x2,y-y2,z-z};
    radius = Vector3Length(dist);

    wrThis->set_afBoundCenter(0, x, wrThis);
    wrThis->set_afBoundCenter(1, y, wrThis);
    wrThis->set_afBoundCenter(2, z, wrThis);
    wrThis->set_fBoundRadius(radius, wrThis);
}
