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
//  GameObject.hpp -                                                      //
//    An object in the game scene graph. These objects inherit from the   //
//    STM object and contain transactional property objects.              //
//                                                                        //
//    The objects have operations to traverse the tree and manage         //
//    children nodes.                                                     //
//                                                                        //
//    For example, the GameObject property will pass down its data.       //
//                                                                        //
//    Animation properties will change the orientation of the object.     //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __GAMEOBJECT_H__
#define __GAMEOBJECT_H__

#include "Game.hpp"

#define MAX_CHILDREN 10

class Spatial;
class GameMaterial;
class GameObject;
class GameAnimation;
class GameObjectController;
class GameObjectBound;
class GamePhysicsProperty;

class GameObject : public Object
{
    // -- RSTM API Generated fields ------------------------------------- //
    GENERATE_FIELD(sh_ptr<Spatial>,              pkSpatialProperty);
    GENERATE_FIELD(sh_ptr<GameMaterial>,         pkMaterialProperty);
    GENERATE_FIELD(sh_ptr<GameAnimation>,        pkAnimationProperty);
    GENERATE_FIELD(sh_ptr<GameObjectController>, pkControllerProperty);
    GENERATE_FIELD(sh_ptr<GamePhysicsProperty>,  pkPhysicsProperty);
    GENERATE_FIELD(sh_ptr<GameObjectBound>,      pkBoundProperty);
    GENERATE_FIELD(unsigned long long,           ullLastTimeStamp);
    GENERATE_FIELD(bool,                         bActive);

    GENERATE_ARRAY(sh_ptr<GameObject>,           children, MAX_CHILDREN);

  public:

      GameObject();

    // To be called after creating a new object.
    void InitProperties(un_ptr<GameObject> unThis,
                        sh_ptr<Spatial> pkNewSpatialProperty,
                        sh_ptr<GameMaterial> pkNewMaterialProperty,
                        sh_ptr<GameAnimation>  pkNewAnimationProperty,
                        sh_ptr<GameObjectController> pkNewControllerProperty,
                        sh_ptr<GamePhysicsProperty> pkNewPhysicsProperty,
                        sh_ptr<GameObjectBound> pkNewBoundProperty);

    // Called on a child.
    void Update(wr_ptr<GameObject> wrThis,
                rd_ptr<GameObject> parent,
                unsigned long long ullTimeStamp,
                int iThreadID = -1);

    // Called on a parent.
    bool Update(wr_ptr<GameObject> wrThis,
                unsigned long long ullTimeStamp,
                int iThreadID = -1);

    void UpdateChildren(wr_ptr<GameObject> wrThis,
                        unsigned long long ullTimeStamp,
                        int iThreadID = -1);

    bool IsActive(rd_ptr<GameObject> rdThis) const;
    void SetActive(wr_ptr<GameObject> wrThis, unsigned long long ullTimeStamp, bool bActive);
    void SetActive(un_ptr<GameObject> unThis, bool bActive);

    void StartAnimation(wr_ptr<GameObject> wrThis,
                        unsigned int iAnimationID,
                        unsigned long long ullTimeStamp,
                        bool overwrite = false,
                        float fFadeInTime = 0.0f,
                        bool repeat = false);

    // Returns false if no child was added (out of space)
    bool AddChild(un_ptr<GameObject> unThis, sh_ptr<GameObject> child);

    // When we don't have inevitability, we need to transactionally leak all
    // of the information required to render a segment.  This struct stores
    // all of the data that must be leaked.
    struct RenderableSegment
    {
      float   m_materialDiffuse[4];
      Matrix4 m_transformation;
      Vector3 m_vec3BoundCorner;
      float   m_fBoundRadius;
      int     m_type;
    };

    // Render this and all children. This will draw a visual object.

    // This version assumes the transaction is inev, and renders
    // directly without buffering.
    void Render(rd_ptr<GameObject> rdThis) const;

    // This version assumes the transaction is not inev and copies
    // all date into vSegmentBuffer to be rendered later.
    void ExtractRender(rd_ptr<GameObject> rdThis,
                       std::vector<RenderableSegment>& vSegmentBuffer) const;



    void ExtractRenderData(rd_ptr<GameObject> rdThis,
                           RenderableSegment& s) const;

    static void RenderSegment(const RenderableSegment& s);
};


#endif // __GAMEOBJECT_H__
