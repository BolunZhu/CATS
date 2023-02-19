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

#include "UpdateThread.hpp"

void UpdateObject(sh_ptr<GameObject> shObject,
                  unsigned long long ullTimeStamp,
                  unsigned int uiTargetIndex)
{
    wr_ptr<GameObject>wrObject(shObject);
    wrObject->Update(wrObject,ullTimeStamp,uiTargetIndex);
}

/* CollideObject inside a transaction */
void txCollideObject(sh_ptr<GameObject> shObject, unsigned int uiTargetIndex)
{
   float x, y, z, radius = -1;

   rd_ptr<GameObject> rdMyObject(shObject);
   sh_ptr<GameObjectBound> shBound = rdMyObject->get_pkBoundProperty(rdMyObject);

   if (shBound) {
      // leak my bound... I'm not going anywhere
      rd_ptr<GameObjectBound> rdBound(shBound);
      rdBound->GetBound(rdBound, x, y, z, radius);
   }

   if (radius == -1) {
      return;
   }

   std::vector< sh_ptr<GameObject> > potentials, colliders;

   potentials =
       WORLD.GetObjectsInsideBound(x, y, z, radius, shObject, uiTargetIndex);

  for (unsigned int i = 0; i < potentials.size(); i++) {
     bool intersects = false;

     rd_ptr<GameObject> rdCurrent(potentials[i]);
     rd_ptr<GameObjectBound> rdBound(rdCurrent->get_pkBoundProperty(rdCurrent));

     if (rdBound->IntersectsBound(rdBound, radius, x, y, z)) {
        intersects = true;
     }

     if (intersects)
        colliders.push_back(potentials[i]);
  }

  for (unsigned int i = 0; i < colliders.size(); i++) {
     rd_ptr<GameObject> rdMyObject(shObject);
     sh_ptr<GameObjectController> shControl(rdMyObject->get_pkControllerProperty(rdMyObject));

     if (shControl)
     {
        wr_ptr<GameObjectController> wrControl(shControl);
        wrControl->OnMsg(wrControl,GameObjectController::GOCM_COLLIDED, &colliders[i], shObject);
     }
  }
}

// -- UpdateObject_BigInev ---------------------------------------------- //
//  Update shObject from thread uiTargetIndex. Try one big update txn using
//  inev.
void UpdateObject_BigInev(sh_ptr<GameObject> shObject,
                          unsigned long long ullTS,
                          unsigned int uiTargetIndex)
{
   bool bTransInev = false;
   BEGIN_TRANSACTION

   if (!GAMECONFIG.m_bNoUpdateInev)
      bTransInev = stm::try_inevitable();

   if (!bTransInev)
   {
      PROF.LogEvent(Profiler::PROF_FAIL_INEV, uiTargetIndex);
   }

   wr_ptr<GameObject> wrCurrent(shObject);
   if (wrCurrent->Update(wrCurrent, ullTS, uiTargetIndex))
   {
       txCollideObject(shObject, uiTargetIndex);
   }
   END_TRANSACTION

   PROF.LogEvent(Profiler::PROF_COMMIT_UPDATE, uiTargetIndex);
}


/* Update with collision detection and visual object update all in
 * one thread.  This is called by the execution thread for each worker. If
 * GAMECONFIG.m_bSplitCollisionTrans is set to false (true by default) this
 * function is called. (See UpdateWorker_SplitCollisions for general
 * information on what the UpdateWorker functions do)
 */
static void UpdateWorker_NoSplit(unsigned int uiTargetIndex)
{
    /* Loop untill gIsGameOver returns true.
     */
    while (!gIsGameOver())
    {
        unsigned long long ullMyStamp = getElapsedTime();

        for (unsigned int uiActionIndex = uiTargetIndex;
            uiActionIndex < GAMECONFIG.m_uiTotalObjects;
            uiActionIndex += GAMECONFIG.m_uiNumThreads)
        {
            if (GAMESTATE.akGameObjects[uiActionIndex]) {
                UpdateObject_BigInev(GAMESTATE.akGameObjects[uiActionIndex],
                                     ullMyStamp,
                                     uiTargetIndex);
                PROF.LogEvent(Profiler::PROF_VISUAL_OBJECT_UPDATE, uiTargetIndex);
            }
        }
        PROF.LogEvent(Profiler::PROF_BATCH_UPDATE,uiTargetIndex);
    }

    /* Thread is about to terminate, call stm lib shutdown that
     * will spit out profiling information.
     */
    stm::shutdown(uiTargetIndex);
}


void GameUpdateThread::operator()()
{
    UpdateWorker_NoSplit(m_uiTargetIndex);
}
