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
//  Spatial.hpp -                                                         //
//    Game object that can move on the game's                             //
//    coordinate system. This class represents                            //
//    things that can move at a base level.                               //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __SPATIAL_H__
#define __SPATIAL_H__

#include "Game.hpp"

class GameObject;

class Spatial : public Object
{

    // -- RSTM API Generated fields -----------//
    GENERATE_ARRAY(float, afWorldTransform, 16);
    GENERATE_ARRAY(float, afLocalTransform, 16);

  public:

    // get the xyz world translation position
    void GetTranslation(rd_ptr<Spatial> rdThis,
                        float& x, float& y, float& z) const;

    // get the xyz world coordinate of passed in local coord
    void TransformPoint(rd_ptr<Spatial> rdThis,
                        float& x, float& y, float& z) const;

    // move the xyz translation position relative to parents
    // if this object has no parent this sets the world translation
    void MoveTranslation(wr_ptr<Spatial> wrThis,
                         float x, float y, float z);

    // set the xyz translation position relative to parents
    // if this object has no parent this sets the world translation
    void SetTranslation(wr_ptr<Spatial> wrThis,
                        float x, float y, float z);

    // set the object to have the following vectors
    // these are relative to its parent unless it has no
    // parent (does not affect translation)
    void SetVectors(wr_ptr<Spatial> wrThis,
                    Vector3 lookAt,
                    Vector3 up,
                    Vector3 right);

    // use to move objects position relative to parents
    // if no parents, use this as the world matrix
    void SetTransformation(wr_ptr<Spatial> wrThis,
                           const Matrix4 mNewTransform);
    void SetTransformation(un_ptr<Spatial> unThis,
                           const Matrix4 mNewTransform);

    void Update(wr_ptr<Spatial> wrThis,
                rd_ptr<GameObject> rd_parent,
                const Matrix4 m4Modifier,
                int iThreadID = -1);

    void Update(wr_ptr<Spatial> wrThis, const Matrix4 m4Modifier,
                int iThreadID = -1);

    const float * GrabWorldMatrix( ) const { return m_afWorldTransform; } ;


  private:
    void SetWorldTransformation(wr_ptr<Spatial> wrThis,
                                const Matrix4 mNewTransform);
};


#endif // __SPATIAL_H__
