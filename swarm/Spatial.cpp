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

#include "Spatial.hpp"

using namespace std;

void Spatial::Update(wr_ptr<Spatial> wrThis, const Matrix4 m4Modifier,
                     int iThreadID)
{
    Matrix4 mLocal;
    for (unsigned int i = 0; i < 16; i++)
    {
        mLocal[i] = wrThis->get_afLocalTransform(i, wrThis);
    }

    Matrix4 mMyWorld;
    Matrix4Multiply(mMyWorld, m4Modifier, mLocal);

    SetWorldTransformation(wrThis, mMyWorld);
}

void Spatial::Update(wr_ptr<Spatial> wrThis,
                     rd_ptr<GameObject> rd_parent,
                     const Matrix4 m4Modifier,
                     int iThreadID)
{
    rd_ptr<Spatial> rd_parentSpatial =
        rd_ptr<Spatial>(rd_parent->get_pkSpatialProperty(rd_parent));

    Matrix4 mParentWorld;
    for (unsigned int i = 0; i < 16; i++)
    {
        mParentWorld[i] =
            rd_parentSpatial->get_afWorldTransform(i, rd_parentSpatial);
    }

    Matrix4 mLocal;
    for (unsigned int i = 0; i < 16; i++)
    {
        mLocal[i] = wrThis->get_afLocalTransform(i, wrThis);
    }

    Matrix4 mMyWorld;
    Matrix4Multiply(mMyWorld, m4Modifier, mLocal);
    Matrix4Multiply(mMyWorld, mMyWorld, mParentWorld);

    SetWorldTransformation(wrThis, mMyWorld);
}

void Spatial::SetTranslation(wr_ptr<Spatial> wrThis,
                             float x,
                             float y,
                             float z)
{
    wrThis->set_afLocalTransform(12, x, wrThis);
    wrThis->set_afLocalTransform(13, y, wrThis);
    wrThis->set_afLocalTransform(14, z, wrThis);
}

void Spatial::MoveTranslation(wr_ptr<Spatial> wrThis,
                              float x,
                              float y,
                              float z)
{
    wrThis->set_afLocalTransform(12,
                                 wrThis->get_afLocalTransform(12, wrThis) + x,
                                 wrThis);


    wrThis->set_afLocalTransform(13,
                                 wrThis->get_afLocalTransform(13, wrThis) + y,
                                 wrThis);

    wrThis->set_afLocalTransform(14,
                                 wrThis->get_afLocalTransform(14, wrThis) + z,
                                 wrThis);
}

void Spatial::GetTranslation(rd_ptr<Spatial> rdThis,
                             float& x,
                             float& y,
                             float& z) const
{
    x = rdThis->get_afLocalTransform(12, rdThis);
    y = rdThis->get_afLocalTransform(13, rdThis);
    z = rdThis->get_afLocalTransform(14, rdThis);
}

void Spatial::TransformPoint(rd_ptr<Spatial> rdThis,
                             float& x, float& y, float& z) const
{
    Matrix4 mWorld;
    for (unsigned int i = 0; i < 16; i++)
    {
        mWorld[i] = rdThis->get_afWorldTransform(i, rdThis);
    }

    Matrix4TransformCoordinates(mWorld,x,y,z);
}

void Spatial::SetVectors(wr_ptr<Spatial> wrThis,
                         Vector3 lookAt,
                         Vector3 up,
                         Vector3 right)
{
    wrThis->set_afLocalTransform(0,right[0], wrThis);
    wrThis->set_afLocalTransform(1,right[1], wrThis);
    wrThis->set_afLocalTransform(2,right[2], wrThis);

    wrThis->set_afLocalTransform(4,up[0], wrThis);
    wrThis->set_afLocalTransform(5,up[1], wrThis);
    wrThis->set_afLocalTransform(6,up[2], wrThis);

    wrThis->set_afLocalTransform(8,lookAt[0], wrThis);
    wrThis->set_afLocalTransform(9,lookAt[1], wrThis);
    wrThis->set_afLocalTransform(10,lookAt[2], wrThis);
}


void Spatial::SetTransformation(wr_ptr<Spatial> wrThis,
                                const Matrix4 mNewTrans)
{
    for (unsigned int i = 0; i < 16; i++)
    {
        wrThis->set_afLocalTransform(i, mNewTrans[i], wrThis);
    }
}

void Spatial::SetTransformation(un_ptr<Spatial> unThis,
                                const Matrix4 mNewTrans)
{
    for (unsigned int i = 0; i < 16; i++)
    {
        unThis->set_afLocalTransform(i,  mNewTrans[i], unThis);
    }
}

void Spatial::SetWorldTransformation(wr_ptr<Spatial> wrThis,
                                     const Matrix4 mNewTrans)
{
    for (unsigned int i = 0; i < 16; i++)
    {
        wrThis->set_afWorldTransform(i, mNewTrans[i], wrThis);
    }
}
