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

#include "Animation.hpp"

// -- BasicAnimation ---------------------------------------------------- //

void BasicAnimation::operator =(const BasicAnimation& other)
{
    clearKeyframes();
    for (unsigned int i = 0; i < other.m_uiNumKeyframes; i++) {
        addKeyframe(other.m_aKeyframes[i]);
    }
}

void BasicAnimation::GetMatrixForTime(Matrix4 m4Result,
                                      float fElapsedTime) const
{
    if (m_uiNumKeyframes == 0)
        return;

    // If no time has passed return the first keyframe.
    if (fElapsedTime == 0)
    {
        Matrix4Set(m4Result, m_aKeyframes[0].m_m4Key);
        return;
    }

    // This an O(n) search. It could be made binary. However since in this
    // demo, the average number of Keyframes in an animation is 4, there
    // would be little benefit to optimization.
    unsigned i = 0;
    while (i < m_uiNumKeyframes && m_aKeyframes[i].m_fTimeIndex < fElapsedTime)
    {
        i++;
    }

    // If the ElapsedTime is larger than the last Keyframe's index, we return
    // the last Keyframe's Key.
    if (i == m_uiNumKeyframes)
    {
        Matrix4Set(m4Result, m_aKeyframes[m_uiNumKeyframes-1].m_m4Key);
        return;
    }

    // Calculate what percentage of the time has passed between the two
    // Keyframes.
    float fInterpPercent = fElapsedTime - m_aKeyframes[i-1].m_fTimeIndex;
    fInterpPercent /=   m_aKeyframes[i].m_fTimeIndex
                      - m_aKeyframes[i-1].m_fTimeIndex;

    // Use linear interpolation (LERP) to find the result matrix. Note that
    // this method will add artifacts to rendering. For rotations it is best to
    // use spherical linear interpolation (SLERP). However it looks good enough
    // for our app and the computation required would be minimal for the
    // correct solution
    for (unsigned int x = 0; x < 16; x++)
    {
        m4Result[x] = fInterpPercent  * m_aKeyframes[i].m_m4Key[x] +
              (1.0f - fInterpPercent) * m_aKeyframes[i-1].m_m4Key[x];
    }

}

bool BasicAnimation::isFinished(float fElapsedTime) const
{
    // if there are no Keyframes or the ElapsedTime is greater than the last
    // Keyframe's TimeIndex the animaiton is finished.
    return (m_uiNumKeyframes == 0 ||
        fElapsedTime > m_aKeyframes[m_uiNumKeyframes-1].m_fTimeIndex);
}

// Slightly inefficient in that it requires a copy. However it is called about
// 8 times in the current app so it is not a target for meaningful
// optimization.
void BasicAnimation::addKeyframe(const Keyframe &kFrame)
{
    Keyframe* newArr = new Keyframe[m_uiNumKeyframes+1];

    if (m_aKeyframes != NULL)
    {
        for (unsigned int i = 0; i < m_uiNumKeyframes; i++)
        {
            newArr[i] = m_aKeyframes[i];
        }
        delete[] m_aKeyframes;
    }
    m_aKeyframes = newArr;
    m_aKeyframes[m_uiNumKeyframes] = kFrame;
    m_uiNumKeyframes++;
}

void BasicAnimation::clearKeyframes()
{
    if (m_aKeyframes)
        delete[] m_aKeyframes;

    m_aKeyframes = NULL;
    m_uiNumKeyframes = 0;
};


// -- GameAnimation ----------------------------------------------------- //

// Static members.
BasicAnimation* GameAnimation::m_akAnimations = NULL;
unsigned int GameAnimation::m_uiNumAnimations = 0;


// -- Animation Management ---------------------------------------------- //

// Again, we are doing an alloc and copy here. This is only called a few
// times though.
void GameAnimation::AddAnimation(const BasicAnimation &kNewAnimation)
{
    BasicAnimation* newArr = new BasicAnimation[m_uiNumAnimations+1];
    if (m_akAnimations != NULL)
    {
        for (unsigned int i = 0; i < m_uiNumAnimations; i++)
        {
            newArr[i] = m_akAnimations[i];
        }
        delete[] m_akAnimations;
    }
    m_akAnimations = newArr;
    newArr[m_uiNumAnimations] = kNewAnimation;

    m_uiNumAnimations++;
}


// -- Animation Control ----------------------------------------- //

bool GameAnimation::StartAnimation(wr_ptr<GameAnimation> wrThis,
                                   unsigned int uiAnimation,
                                   unsigned long long ullTimeStamp,
                                   bool overwrite)
{
    if (!overwrite)
    {
        if (wrThis->get_bIsAnimating(wrThis))
        {
            return false;
        }
    }

    if (uiAnimation >= m_uiNumAnimations)
    {
        return false;
    }

    // Simply overwrite or start the animation.
    wrThis->set_bIsAnimating(true, wrThis);
    wrThis->set_iAnimationIndex(uiAnimation, wrThis);
    wrThis->set_ullStartTime(ullTimeStamp, wrThis);
    wrThis->set_bIsRepeating(false, wrThis); // Always false for now.

    return true;
}

void GameAnimation::Update(wr_ptr<GameAnimation> wrThis,
                           wr_ptr<GameObject> wrMyObject,
                           Matrix4 m4Result,
                           unsigned long long ullTimeStamp,
                           int iThreadID)
{
    if (!wrThis->get_bIsAnimating(wrThis))
        return;

    sh_ptr<Spatial> spkSpatialProperty =
        wrMyObject->get_pkSpatialProperty(wrMyObject);

    if (!spkSpatialProperty)
        return;

    float fPassedTime = getNumSecPassed(ullTimeStamp,
                                        wrThis->get_ullStartTime(wrThis));

    unsigned int myAnimation = wrThis->get_iAnimationIndex(wrThis);

    if (m_akAnimations[myAnimation].isFinished(fPassedTime))
        wrThis->set_bIsAnimating(false, wrThis);

    m_akAnimations[myAnimation].GetMatrixForTime(m4Result, fPassedTime);
}
