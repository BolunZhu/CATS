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
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <iostream>
 using namespace std;

#include "GameObject.hpp"

GameObject::GameObject() : m_ullLastTimeStamp(RENDER.GrabTimeStamp()) {
    for (unsigned int i = 0; i < MAX_CHILDREN; i++)
        m_children[i] = sh_ptr<GameObject>(NULL);
}


void
GameObject::InitProperties(un_ptr<GameObject> unThis,
                           sh_ptr<Spatial> pkNewSpatialProperty,
                           sh_ptr<GameMaterial> pkNewMaterialProperty,
                           sh_ptr<GameAnimation> pkNewAnimationProperty,
                           sh_ptr<GameObjectController>
                           pkNewControllerProperty,
                           sh_ptr<GamePhysicsProperty> pkNewPhysicsProperty,
                           sh_ptr<GameObjectBound> pkNewBoundProperty)
{
    unThis->set_pkSpatialProperty(pkNewSpatialProperty, unThis);
    unThis->set_pkMaterialProperty(pkNewMaterialProperty, unThis);
    unThis->set_pkAnimationProperty(pkNewAnimationProperty, unThis);
    unThis->set_pkControllerProperty(pkNewControllerProperty, unThis);
    unThis->set_pkPhysicsProperty(pkNewPhysicsProperty, unThis);
    unThis->set_pkBoundProperty(pkNewBoundProperty, unThis);
    unThis->set_bActive(true, unThis);

    // Make sure for the first update we don't time skip a great amount.
    unThis->set_ullLastTimeStamp(RENDER.GrabTimeStamp(), unThis);
}


void GameObject::Update(wr_ptr<GameObject> wrThis,
                        rd_ptr<GameObject> rd_parent,
                        unsigned long long ullTimeStamp,
                        int iThreadID)
{
    // First grab properties.
    sh_ptr<GameObjectBound> spkBoundProperty =
        wrThis->get_pkBoundProperty(wrThis);

    sh_ptr<Spatial> spkSpatialProperty =
        wrThis->get_pkSpatialProperty(wrThis);

    wr_ptr<Spatial> wr_pkSpatial(spkSpatialProperty);

    Matrix4 m4Result;
    Matrix4Identity(m4Result);

    sh_ptr<GameAnimation> spkAnimationProperty =
        wrThis->get_pkAnimationProperty(wrThis);

    // If this object has an animation, update it.
    wr_ptr<GameAnimation> wr_pkAnimation(spkAnimationProperty);
    if (spkAnimationProperty) {
        wr_pkAnimation->Update(wr_pkAnimation, 
                               wrThis, 
                               m4Result, 
                               ullTimeStamp, 
                               iThreadID);
        tx_release(wr_pkAnimation);
    }

    // If this object has a spatial, update it.
    if (spkSpatialProperty) {
        wr_pkSpatial->Update(wr_pkSpatial, rd_parent, m4Result, iThreadID);

        // Also update the bound if it exists.
        if (spkBoundProperty) {
            wr_ptr<GameObjectBound> wr_pkBound(spkBoundProperty);
            wr_pkBound->UpdateBound(wr_pkBound,wr_pkSpatial, iThreadID);
            tx_release(wr_pkBound);
        }
    }

    tx_release(rd_parent);
    UpdateChildren(wrThis, ullTimeStamp, iThreadID);
}

bool GameObject::Update(wr_ptr<GameObject> wrThis, 
                        unsigned long long ullTimeStamp, 
                        int iThreadID)
{
    if (!IsActive(wrThis))
        return false;

    // Figure out how much time has passed to make sure we move by the 
    // correct amount.
    unsigned long long ullLastTimeStamp = wrThis->get_ullLastTimeStamp(wrThis);
    float fElapsedTime = getNumSecPassed(ullTimeStamp, ullLastTimeStamp);

    if (fElapsedTime < .00001f)
    {
        PROF.LogEvent(Profiler::PROF_SKIP_NO_TIME_PASSED, iThreadID);
        return false;
    }


    // Get properties.
    sh_ptr<Spatial> spkSpatialProperty =
        wrThis->get_pkSpatialProperty(wrThis);

    sh_ptr<GameAnimation> spkAnimationProperty =
        wrThis->get_pkAnimationProperty(wrThis);

    sh_ptr<GameObjectController> spkControllerProperty =
        wrThis->get_pkControllerProperty(wrThis);

    sh_ptr<GamePhysicsProperty> spkPhysicsProperty =
        wrThis->get_pkPhysicsProperty(wrThis);

    sh_ptr<GameObjectBound> spkBoundProperty =
        wrThis->get_pkBoundProperty(wrThis);

    float boundRadius;
    float boundX, boundY, boundZ;

    // If the object has an animation, update it.
    Matrix4 m4AnimationResult;
    Matrix4Identity(m4AnimationResult);

    if (spkAnimationProperty) {
        wr_ptr<GameAnimation> wr_pkAnimation(spkAnimationProperty);
        wr_pkAnimation->Update(wr_pkAnimation, 
                               wrThis, 
                               m4AnimationResult,
                               ullTimeStamp, 
                               iThreadID);
        tx_release(wr_pkAnimation);
    }

    // If the object has a controller, update it.
    if (spkControllerProperty) {
        wr_ptr<GameObjectController> wr_pkController(spkControllerProperty);
        wr_pkController->UpdateState(wr_pkController,
                                     wrThis, 
                                     ullTimeStamp,
                                     iThreadID);
        tx_release(wr_pkController);
    }

    // If the object has physics, update it.
    if (spkPhysicsProperty) {
        wr_ptr<GamePhysicsProperty> wr_pkPhysics(spkPhysicsProperty);
        wr_pkPhysics->UpdateProperty(wr_pkPhysics, 
                                     wrThis, 
                                     fElapsedTime, 
                                     iThreadID);
        tx_release(wr_pkPhysics);
    }

    // Update the spatial, note that the result of the animation is passed 
    // in.
    if (spkSpatialProperty) {
        wr_ptr<Spatial> wr_pkSpatial(spkSpatialProperty);
        wr_pkSpatial->Update(wr_pkSpatial, m4AnimationResult, iThreadID);

        if (spkBoundProperty) {

            wr_ptr<GameObjectBound> wr_pkBound(spkBoundProperty);
            wr_pkBound->GetBound(wr_pkBound, 
                                 boundX, boundY, boundZ, boundRadius);
            wr_pkBound->UpdateBound(wr_pkBound,wr_pkSpatial, iThreadID);
        }

        tx_release(wr_pkSpatial);
    }

    // Update the children.
    UpdateChildren(wrThis, ullTimeStamp, iThreadID);

    // Rehash this object in the world if nessicary.
    if (spkBoundProperty) {
        rd_ptr<GameObjectBound> rd_pkBound(spkBoundProperty);
        if (rd_pkBound->ChangedBox(rd_pkBound)) {
            wr_ptr<GameObjectBound> wr_pkBound(spkBoundProperty);
            WORLD.UpdateBound(wrThis,wr_pkBound,iThreadID);
        }

        tx_release(rd_pkBound);
    }

    // Replace the time stamp.
    wrThis->set_ullLastTimeStamp(ullTimeStamp, wrThis);

    return true;
}

void GameObject::UpdateChildren(wr_ptr<GameObject> wrThis,
                                unsigned long long ullTimeStamp,
                                int iThreadID)
{
    // We propagate bounds up to the root of the object.
    sh_ptr<GameObjectBound> spkBoundProperty =
        wrThis->get_pkBoundProperty(wrThis);

    for (unsigned int i = 0; i < MAX_CHILDREN; i++)
    {
        if (wrThis->get_children(i,wrThis) != NULL)
        {
            wr_ptr<GameObject> child =
                wr_ptr<GameObject>(wrThis->get_children(i, wrThis));
            child->Update(child, wrThis, ullTimeStamp, iThreadID);

            sh_ptr<GameObjectBound>
                spkChildBound(child->get_pkBoundProperty(child));

            if (spkBoundProperty && spkChildBound)
            {
                wr_ptr<GameObjectBound> wr_pkBound(spkBoundProperty);
                rd_ptr<GameObjectBound> rdChildBound(spkChildBound);
                wr_pkBound->ExpandBound(wr_pkBound,rdChildBound);
            }

            tx_release(child);
        }
    }
}

void GameObject::SetActive(wr_ptr<GameObject> wrThis, unsigned long long ullTimeStamp, bool bActive)
{
    wrThis->set_ullLastTimeStamp(ullTimeStamp, wrThis);
    wrThis->set_bActive(bActive, wrThis);
}

void GameObject::SetActive(un_ptr<GameObject> unThis, bool bActive)
{
    unThis->set_ullLastTimeStamp(RENDER.GrabTimeStamp(), unThis);
    unThis->set_bActive(bActive, unThis);
}

bool GameObject::IsActive(rd_ptr<GameObject> rdThis) const
{
    return(rdThis->get_bActive(rdThis));
}

void GameObject::StartAnimation(wr_ptr<GameObject> wrThis,
                                unsigned int iAnimationID,
                                unsigned long long ullTimeStamp,
                                bool overwrite,
                                float fFadeInTime,
                                bool repeat)
{
    sh_ptr<GameAnimation> spkAnimationProperty =
        wrThis->get_pkAnimationProperty(wrThis);

    if (spkAnimationProperty)
    {
        wr_ptr<GameAnimation> wr_pkAnimation(spkAnimationProperty);
        wr_pkAnimation->StartAnimation(wr_pkAnimation, iAnimationID, ullTimeStamp, overwrite);
        tx_release(wr_pkAnimation);
    }

    for (unsigned int i = 0; i < MAX_CHILDREN; i++)
    {
        if (wrThis->get_children(i,wrThis) != NULL)
        {

            wr_ptr<GameObject> child(wrThis->get_children(i,wrThis));
            child->StartAnimation(child, iAnimationID, ullTimeStamp, overwrite, fFadeInTime, repeat);
            tx_release(child);
        }
    }

}

bool GameObject::AddChild(un_ptr<GameObject> unThis, sh_ptr<GameObject> child)
{
    for (unsigned int i = 0; i < MAX_CHILDREN; i++)
    {
        sh_ptr<GameObject> unCurrent = unThis->m_children[i];
        if (unCurrent == NULL)
        {
            unThis->set_children(i, child, unThis);
            return true;
        }
    }
    return false;
}

// This renders this object and its children from inside a transaction
// assuming we can use INEV. This will produce visual artifacts if
// INEV is not available.
void GameObject::Render(rd_ptr<GameObject> rdThis) const
{
    if (!rdThis->IsActive(rdThis))
    {
        cout << "DEBUG: Inactive object found..."
             << "  this should never happen." << endl;
        return;
    }

    sh_ptr<Spatial> spkSpatialProperty =
        rdThis->get_pkSpatialProperty(rdThis);

    sh_ptr<GameMaterial> spkMaterialProperty =
        rdThis->get_pkMaterialProperty(rdThis);

    sh_ptr<GameObjectBound> spkBoundProperty =
        rdThis->get_pkBoundProperty(rdThis);

    sh_ptr<GameObjectController> spkController =
        rdThis->get_pkControllerProperty(rdThis);

    sh_ptr<GamePhysicsProperty> spkPhysicsProperty =
        rdThis->get_pkPhysicsProperty(rdThis);

    const float * diffuse = NULL;
    const float * transform = NULL;
    const float * boundCenter = NULL;

    int iType = 1;
    if (spkController)
    {
        rd_ptr<GameObjectController> rdController(spkController);
        if (rdController->get_iState(rdController) ==
            GameObjectController::GOCS_FLOAT)
        {
            iType = 2;
        }
    }

    if (spkSpatialProperty)
    {
        rd_ptr<Spatial> rdSpatial(spkSpatialProperty);
        // 16 float array
        transform = rdSpatial->GrabWorldMatrix();
        stm::inev_read_prefetch(transform, 16*sizeof(float));
    }

    if (m_pkMaterialProperty)
    {
        rd_ptr<GameMaterial> rdMaterial(spkMaterialProperty);
        // 4 float array
        diffuse = rdMaterial->GrabDiffuse();
        stm::inev_read_prefetch(diffuse, 4*sizeof(float));
    }

    float m_fBoundRadius = -1.0f;
    if (m_pkBoundProperty)
    {
        rd_ptr<GameObjectBound> rdBound(spkBoundProperty);
        m_fBoundRadius = rdBound->get_fBoundRadius(rdBound);
        // 3 float array
        boundCenter = rdBound->GrabBoundCenter();
        stm::inev_read_prefetch(boundCenter, 3*sizeof(float));
    }

    // if we had either no spatial or material property,
    // our arrays are null, so we don't render. (Never happens as of now)
    if (transform && diffuse)
    {
        RENDER.DrawMesh(transform, diffuse, iType);
    }

    if (boundCenter)
    {
        RENDER.DrawBound(boundCenter, m_fBoundRadius);
    }

    for (unsigned int i = 0; i < MAX_CHILDREN; i++)
    {
        sh_ptr<GameObject> shCurrent(rdThis->get_children(i,rdThis));
        if (shCurrent)
        {
            rd_ptr<GameObject> rdCurrent(shCurrent);
            rdCurrent->Render(rdCurrent);
        }
    }
}

// This version assumes the transaction is not inev and copies
// all data into vSegmentBuffer to be rendered later.
void GameObject::ExtractRender(rd_ptr<GameObject> rdThis,
                               std::vector<RenderableSegment>& vSegmentBuffer) const
{
    RenderableSegment oCurrentSegment;
    // Initialize the parts of the returned segment that aren't always assigned
    // below
    oCurrentSegment.m_vec3BoundCorner[0] =
        oCurrentSegment.m_vec3BoundCorner[1] =
        oCurrentSegment.m_vec3BoundCorner[2] = 0;

    sh_ptr<Spatial> spkSpatialProperty =
        rdThis->get_pkSpatialProperty(rdThis);

    sh_ptr<GameMaterial> spkMaterialProperty =
        rdThis->get_pkMaterialProperty(rdThis);

    sh_ptr<GameObjectBound> spkBoundProperty =
        rdThis->get_pkBoundProperty(rdThis);

    sh_ptr<GameObjectController> spkController =
        rdThis->get_pkControllerProperty(rdThis);

    tx_release(rdThis);

    oCurrentSegment.m_materialDiffuse[0] = 0.5f;
    oCurrentSegment.m_materialDiffuse[1] = 0.5f;
    oCurrentSegment.m_materialDiffuse[2] = 0.5f;
    oCurrentSegment.m_materialDiffuse[3] = 1.0f;

    Matrix4Identity(oCurrentSegment.m_transformation);

    oCurrentSegment.m_type = 1;
    if (spkController)
    {
        rd_ptr<GameObjectController> rdController(spkController);
        if (rdController->get_iState(rdController) ==
            GameObjectController::GOCS_FLOAT)
        {
            oCurrentSegment.m_type = 2;
        }
    }

    if (spkSpatialProperty)
    {
        rd_ptr<Spatial> rdSpatial(spkSpatialProperty);

        for (unsigned int i = 0; i < 16; i++)
        {
            oCurrentSegment.m_transformation[i] =
                rdSpatial->get_afWorldTransform(i, rdSpatial);
        }

        tx_release(rdSpatial);
    }

    if (m_pkMaterialProperty)
    {
        rd_ptr<GameMaterial> rdMaterial(spkMaterialProperty);
        for (int i = 0; i < 4; i++)
        {
            oCurrentSegment.m_materialDiffuse[i] =
                rdMaterial->get_afDiffuse(i, rdMaterial);
        }
        tx_release(rdMaterial);
    }

    oCurrentSegment.m_fBoundRadius = -1.0f;
    if (m_pkBoundProperty)
    {
        rd_ptr<GameObjectBound> rdBound(spkBoundProperty);

        // NB: pass by ref of the matrix fields
        rdBound->GetBound(rdBound,
                          oCurrentSegment.m_vec3BoundCorner[0],
                          oCurrentSegment.m_vec3BoundCorner[1],
                          oCurrentSegment.m_vec3BoundCorner[2],
                          oCurrentSegment.m_fBoundRadius);
        tx_release(rdBound);
    }

    vSegmentBuffer.push_back(oCurrentSegment);

    for (unsigned int i = 0; i < MAX_CHILDREN; i++)
    {
        sh_ptr<GameObject> shCurrent(rdThis->get_children(i,rdThis));
        if(shCurrent)
        {
            rd_ptr<GameObject> rdCurrent(shCurrent);
            rdCurrent->ExtractRender(rdCurrent,vSegmentBuffer);
        }
    }
}

void GameObject::RenderSegment(const RenderableSegment& s)
{
    RENDER.DrawMesh(s.m_transformation,
                    s.m_materialDiffuse,
                    s.m_type);

    RENDER.DrawBound(s.m_vec3BoundCorner,
                     s.m_fBoundRadius);
}
