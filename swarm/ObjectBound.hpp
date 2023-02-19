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
//  GameObjectBound.hpp -                                                 //
//    Bounding sphere for culling, collision detection, and picking       //
//    objects with a ray.                                                 //
//                                                                        //
//    These will be set by passing information up the game object tree.   //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __GAMEOBJECTBOUND_H__
#define __GAMEOBJECTBOUND_H__

#include "Game.hpp"

class Spatial;

class GameObjectBound : public Object
{

    // -- RSTM API Generated fields --------------------------------- //
    // local coordinates relative to parent
    GENERATE_ARRAY(float, afLocalBoundCenter, 3);
    GENERATE_FIELD(float, fLocalBoundRadius);

    GENERATE_FIELD(int, iMyBox);


    GENERATE_ARRAY(float, afBoundCenter, 3);
    GENERATE_FIELD(float, fBoundRadius);

  public:

    GameObjectBound() : m_iMyBox(-1),
                        m_fBoundRadius(1.0f)
    {
    }

    const float * GrabBoundCenter() const { return m_afBoundCenter; };

    // Make my bound include both my bound and
    // rdOther.
    void ExpandBound(wr_ptr<GameObjectBound> wrThis,
                     rd_ptr<GameObjectBound> rdOther);

    bool ChangedBox(rd_ptr<GameObjectBound> rdThis) const;

    bool LineIsInBound(rd_ptr<GameObjectBound> rdThis,
                       float startX, float startY, float startZ,
                       float endX, float endY, float endZ) const;

    // True if world point x, y, z is in bound.
    bool IsInBound(rd_ptr<GameObjectBound> rdThis,
                   const float& x, const float& y, const float& z) const;

    // True if bounds intersect.
    bool IntersectsBound(rd_ptr<GameObjectBound> rdThis,
                         rd_ptr<GameObjectBound> rdOther) const;
    // True if bounds intersect.
    bool IntersectsBound(rd_ptr<GameObjectBound> rdThis,
                         float radius, float x, float y, float z) const;

    void GetBound(rd_ptr<GameObjectBound> rdThis,
                  float& x, float& y, float& z,
                  float& radius) const;

    // Resets the local bound center and radius.
    void SetBound(un_ptr<GameObjectBound> unThis,
                  float radius, float x, float y, float z);

    // Resets the local bound radius.
    void SetRadius(wr_ptr<GameObjectBound> wrThis,
                   float radius);

    // Resets the local bound center.
    void SetCenter(wr_ptr<GameObjectBound> wrThis,
                   float x, float y, float z);

    // Set the bound to be the translated version of the
    // local bound.
    void UpdateBound(wr_ptr<GameObjectBound> wrThis,
                     rd_ptr<Spatial> rdMyObject,
                     int iThreadID = -1);

    // Set the bound to encompass the current bound
    // as well as the other bound.
    void AddBound(wr_ptr<GameObjectBound> wrThis,
                  rd_ptr<GameObjectBound> rdOtherBound);
};

#endif // __GAMEOBJECTBOUND_H__
