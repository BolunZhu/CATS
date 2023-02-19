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
//  Animation.hpp -                                                       //
//    Animations to move objects based on a set of matrix keyframes.      //                         //
//    These transformations are applied to the transformation matrix      //
//    after it is calculated.                                             //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __ANIMATION_H__
#define __ANIMATION_H__

#include "Game.hpp"

class GameObject;

// -- Keyframe ---------------------------------------------------------- //
//  Animation should be at m_m4Key exactly when m_fTimeIndex is reached.
struct Keyframe
{
    Matrix4 m_m4Key;
    float   m_fTimeIndex;
};


// -- BasicAnimation ---------------------------------------------------- //
//  Basic container of a list of Keyframes. This object can return
//  interpolated frames for time values between Keyframes.
class BasicAnimation
{
  public:
      BasicAnimation() : m_aKeyframes(NULL),
                         m_uiNumKeyframes(0)
      { };

    void operator =(const BasicAnimation& other);

    // Set result to be the interpolation of whatever Keyframes are needed
    // to represent the given time. If fElapsed time is larger than the
    // last keyframe's TimeIndex, then we return the last Key.
    void GetMatrixForTime(Matrix4 m4Result, float fElapsedTime) const;

    // Returns true if the time index is larger than the animation duration.
    bool isFinished(float fElapsedTime) const;

    // Insert a new Keyframe object into the animation. This results in an
    // array copy as of now, but is only called 8 times in the GameState, so
    // it is not a target for meaningful optimization.
    void addKeyframe(const Keyframe &kFrame);

    void clearKeyframes();

  private:
    // Array of Keyframes in ascending order
    Keyframe * m_aKeyframes;
    unsigned int m_uiNumKeyframes;
};

// -- GameAnimation ----------------------------------------------------- //
//  Actual transactional controller for an animation. This will manage a
//  static list of BasicAnimations, and since to find a Keyframe for a
//  given time stamp is read only, we do not need to manage the actual
//  BasicAnimations transactionally.
class GameAnimation : public Object
{
    // -- RSTM API Generated fields --------------------------------- //
    GENERATE_FIELD(bool, bIsAnimating);
    GENERATE_FIELD(bool, bIsRepeating);

    // Info about the currently running animation.
    GENERATE_FIELD(unsigned long long, ullStartTime);
    GENERATE_FIELD(int, iAnimationIndex);

  private:
    // Static list of all animations in game. This is not managed
    // transactionally because we make the assumption that all animation
    // loading takes place on one thread at initialization time.
    static unsigned int m_uiNumAnimations;
    static BasicAnimation* m_akAnimations;

  public:

    // -- Constructor ----------------------------------------------- //
    GameAnimation() : m_bIsAnimating(false),
                      m_bIsRepeating(false),
                      m_ullStartTime(0),
                      m_iAnimationIndex(0)
    { };


    // -- Animation Management -------------------------------------- //
    //  Helper to manage the static list of animations. (See above)
    static void AddAnimation(const BasicAnimation &kNewAnimation);


    // -- Animation Control ----------------------------------------- //

    bool StartAnimation(wr_ptr<GameAnimation> wrThis,
                        unsigned int uiAnimation,
                        unsigned long long ullTimeStamp,
                        bool overwrite = false);


    // Set m4Result to the matrix for whatever TimeStamp is passed.
    // If the animation has finished set bIsAnimating to false.
    void Update(wr_ptr<GameAnimation> wrThis,
                wr_ptr<GameObject> wrMyObject,
                Matrix4 m4Result,
                unsigned long long ullTimeStamp,
                int iThreadID = -1);
};

#endif // __ANIMATION_H__
