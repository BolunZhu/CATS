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

#include "World.hpp"

using namespace std;

GameWorld WORLD;

bool GameWorld::Init()
{
    m_aHashTable.resize(GAMECONFIG.m_uiSpatialHashSize *
                        GAMECONFIG.m_uiSpatialHashSize *
                        GAMECONFIG.m_uiSpatialHashSize);

    for (unsigned int i = 0; i < m_aHashTable.size(); i++) {
        m_aHashTable[i] = sh_ptr<STMList>(new STMList());
    }

    return true;
}

std::vector<int> GameWorld::GetHash(float radius, float x, float y, float z)
{
    radius += 2.5f;
    float leftX, leftY, leftZ, rightX, rightY, rightZ;
    leftX = x - radius;
    leftY = y - radius;
    leftZ = z - radius;

    rightX = x + radius;
    rightY = y + radius;
    rightZ = z + radius;

    int leftHashX = (int)floor(leftX / GAMECONFIG.m_fHashGridWidth);
    int leftHashY = (int)floor(leftY / GAMECONFIG.m_fHashGridWidth);
    int leftHashZ = (int)floor(leftZ / GAMECONFIG.m_fHashGridWidth);

    int rightHashX = (int)ceil(rightX / GAMECONFIG.m_fHashGridWidth);
    int rightHashY = (int)ceil(rightY / GAMECONFIG.m_fHashGridWidth);
    int rightHashZ = (int)ceil(rightZ / GAMECONFIG.m_fHashGridWidth);

    std::vector<int> retVec;

    for (int i = leftHashX; i <= rightHashX; i++) {
        for (int j = leftHashY; j <= rightHashY; j++) {
            for (int k = leftHashZ; k <= rightHashZ; k++) {

                int mod = GAMECONFIG.m_uiSpatialHashSize;
                int box =   abs(i % mod)
                    + abs(j % mod)*mod
                    + abs(k % mod)*mod*mod;
                bool isNew = true;
                for (unsigned int i = 0; i < retVec.size(); i++) {
                    if (retVec[i] == box) {
                        isNew = false;
                        i = (unsigned int)retVec.size();
                    }
                }
                if (isNew)
                    retVec.push_back(box);
            }
        }
    }

    return retVec;
}

std::vector< sh_ptr<GameObject> >
GameWorld::GetObjectsInsideBucket(int bucketIndex)
{
    rd_ptr<STMList> rdBox = rd_ptr<STMList>(m_aHashTable[bucketIndex]);
    return rdBox->GrabAll(rdBox);
}

std::vector< sh_ptr<GameObject> >
GameWorld::GetObjectsInsideBound(rd_ptr<GameObjectBound> rdBound,
                                 sh_ptr<GameObject> myObject,
                                 int iThreadID)
{
    float x, y, z, radius;
    rdBound->GetBound(rdBound,x,y,z,radius);

    return GetObjectsInsideBound(x,y,z,radius,myObject,iThreadID);
}

std::vector< sh_ptr<GameObject> >
GameWorld::GetObjectsInsideBound(float x, float y, float z, float radius,
                                 sh_ptr<GameObject> myObject,
                                 int iThreadID)
{

    std::vector< sh_ptr<GameObject> > retVector;
    std::vector<int> boxVector = GetHash(radius,x,y,z);

    for (unsigned int boxIter = 0; boxIter < boxVector.size(); boxIter++) {
        int box = boxVector[boxIter];

        wr_ptr<STMList> rdBox(m_aHashTable[box]);
        std::vector< sh_ptr<GameObject> > boxVec =
            rdBox->GrabAll(rdBox,iThreadID);

        for (unsigned int i = 0; i < boxVec.size(); i++) {
            if (boxVec[i] != myObject) {
                retVector.push_back(boxVec[i]);
            }
        }
    }
    return retVector;
}

bool GameWorld::AddBound(sh_ptr<GameObject> newObject,
                         float x, float y, float z,
                         float radius,
                         int iThreadID)
{
    int mod = GAMECONFIG.m_uiSpatialHashSize;
    int box =   abs((int)(x / GAMECONFIG.m_fHashGridWidth) % mod)
        + abs((int)(y / GAMECONFIG.m_fHashGridWidth) % mod)*mod
        + abs((int)(z / GAMECONFIG.m_fHashGridWidth) % mod)*mod*mod;

    wr_ptr<STMList> wrBox = wr_ptr<STMList>(m_aHashTable[box]);
    wrBox->Insert(wrBox,newObject,iThreadID);

    return true;
}

int GameWorld::GetBox(float x, float y, float z) const
{
    int mod = GAMECONFIG.m_uiSpatialHashSize;
    int box =   abs((int)(x / GAMECONFIG.m_fHashGridWidth) % mod)
        + abs((int)(y / GAMECONFIG.m_fHashGridWidth) % mod)*mod
        + abs((int)(z / GAMECONFIG.m_fHashGridWidth) % mod)*mod*mod;

    return box;
}

int GameWorld::GetBox(rd_ptr<GameObjectBound> rdBound) const
{
    float x, y, z, r;
    rdBound->GetBound(rdBound,x,y,z,r);
    return GetBox(x,y,z);
}

void GameWorld::UpdateBound(sh_ptr<GameObject> obj,
                            wr_ptr<GameObjectBound> wrBound,
                            int iThreadID)
{
    int oldBox = wrBound->get_iMyBox(wrBound);
    int newBox = GetBox(wrBound);
    wrBound->set_iMyBox(newBox, wrBound);

    if (oldBox >= 0)
    {
        wr_ptr<STMList> wrOldBox = wr_ptr<STMList>(m_aHashTable[oldBox]);
        wrOldBox->Remove(wrOldBox,obj, iThreadID);
    }

    wr_ptr<STMList> wrNewBox = wr_ptr<STMList>(m_aHashTable[newBox]);
    wrNewBox->Insert(wrNewBox,obj,iThreadID);
}


bool GameWorld::RemoveBound(sh_ptr<GameObject> newObject,
                            float x, float y, float z,
                            float radius,
                            int iThreadID)
{
    int mod = GAMECONFIG.m_uiSpatialHashSize;
    int box =   abs((int)(x / GAMECONFIG.m_fHashGridWidth) % mod)
        + abs((int)(y / GAMECONFIG.m_fHashGridWidth) % mod)*mod
        + abs((int)(z / GAMECONFIG.m_fHashGridWidth) % mod)*mod*mod;


    wr_ptr<STMList> wrBox = wr_ptr<STMList>(m_aHashTable[box]);
    wrBox->Remove(wrBox,newObject,iThreadID);

    return true;
}



bool GameWorld::Add(sh_ptr<GameObject> newObject)
{
    m_aWorldObjects.push_back(newObject);
    return true;
}

bool GameWorld::AddGravitationalObject(sh_ptr<GameObject> newObject)
{
    m_aPhysicsObjects.push_back(newObject);
    return true;
}
