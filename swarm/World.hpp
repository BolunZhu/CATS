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
//  GameWorld.hpp -                                                       //
//    This holds a sh pointer to all objects in                           //
//    the game world. If we want to draw any                              //
//    object, it must be first added to the game                          //
//    world.                                                              //
//                                                                        //
//    This is a singleton defined as WORLD                                //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __GAMEWORLD_H__
#define __GAMEWORLD_H__

#include "Game.hpp"

class GameObjectNode;
class GameObject;
class STMList;


class GameWorld
{
  public:
    GameWorld() { }
    ~GameWorld()
    {
        m_aWorldObjects.clear();
        m_aPhysicsObjects.clear();
    }

    bool Init();

    // add an object that "emits" gravity (planet)
    bool AddGravitationalObject(sh_ptr<GameObject>);

    bool Add(sh_ptr<GameObject>);
    bool Remove(sh_ptr<GameObject>);

    int GetBox(float x, float y, float z) const;
    int GetBox(rd_ptr<GameObjectBound> rdBound) const;

    void UpdateBound(sh_ptr<GameObject> obj,
                     wr_ptr<GameObjectBound> wrBound,
                     int iThreadID = -1);

    bool RemoveBound(sh_ptr<GameObject> obj,
                     float x, float y, float z,
                     float radius,
                     int iThreadID = -1);

    bool AddBound(sh_ptr<GameObject> obj,
                  float x, float y, float z,
                  float radius,
                  int iThreadID = -1);

    std::vector< sh_ptr<GameObject> >
    GetObjectsInsideBound(rd_ptr<GameObjectBound> rdBound,
                          sh_ptr<GameObject> shMyObject,
                          int iThreadID = -1);

    std::vector< sh_ptr<GameObject> >
    GetObjectsInsideBound(float x, float y, float z,
                          float radius, sh_ptr<GameObject> shMyObject,
                          int iThreadID = -1);

    void GetCameraPosition(float& x, float& y, float &z);
    void GetCameraLookAt(float& x, float& y, float &z);

    void SetCameraPosition(sh_ptr<GameObject> newPosition);
    void SetCameraLookAt(sh_ptr<GameObject> newLookAt);

    std::vector< sh_ptr<GameObject> > GetObjectsInsideBucket(int bucketIndex);

    const std::vector< sh_ptr<GameObject> > GetWorld()
    { return m_aWorldObjects; };

    const std::vector< sh_ptr<GameObject> > GetGravitationalObjects()
    { return m_aPhysicsObjects; };

  private:

    std::vector< sh_ptr<GameObject> > m_aWorldObjects;

    // list all possible hash boxes that could be inside the bound
    std::vector<int> GetHash(float radius, float x, float y, float z);

    // if these are null, the camera will use a default position and look at

    // the camera will be set to this position each frame
    sh_ptr<GameObject> m_pkCameraPositionNode;

    // the camera will look at this position each frame
    sh_ptr<GameObject> m_pkCameraLookAtNode;

    // these objects emit gravitational fields
    std::vector< sh_ptr<GameObject> > m_aPhysicsObjects;

    // spatial hash collision detection table
    std::vector< sh_ptr<STMList> > m_aHashTable;


};

// define singleton
extern GameWorld WORLD;

#endif // __GAMEWORLD_H__
