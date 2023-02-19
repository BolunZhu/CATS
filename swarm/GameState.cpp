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

#include "GameState.hpp"
#include "UpdateThread.hpp"

using namespace std;

GameState GAMESTATE;

GameState::GameState()
{
    akGameObjects = NULL;
}

GameState::~GameState()
{
    Shutdown();
}

void GameState::JoinAll()
{
   for (unsigned int i = 0; i < akThreads.size(); i++)
   {
      delete akThreads[i];
   }
   akThreads.clear();
}

bool GameState::Init()
{
    // ------ Begin "Loading" ------------------------------------------- //
    //  For now we just run commands, in the future
    //  this will read from file.

    // - - ANIMATION PHASE - - - - - - - - - - - //
    BasicAnimation kFirstAnim;

    Keyframe kNewKeyframe;
    kNewKeyframe.m_fTimeIndex = 0.0f;
    Matrix4Identity(kNewKeyframe.m_m4Key);

    kFirstAnim.addKeyframe(kNewKeyframe);

    kNewKeyframe.m_fTimeIndex = .25f;
    Matrix4RotationY(kNewKeyframe.m_m4Key, 1.0472f);

    kFirstAnim.addKeyframe(kNewKeyframe);

    kNewKeyframe.m_fTimeIndex = 1.0f;
    kFirstAnim.addKeyframe(kNewKeyframe);

    kNewKeyframe.m_fTimeIndex = 2.0f;
    Matrix4Identity(kNewKeyframe.m_m4Key);
    kFirstAnim.addKeyframe(kNewKeyframe);

    GameAnimation::AddAnimation(kFirstAnim);


    kFirstAnim.clearKeyframes();

    kNewKeyframe.m_fTimeIndex = 0.0f;
    Matrix4Identity(kNewKeyframe.m_m4Key);

    kFirstAnim.addKeyframe(kNewKeyframe);

    kNewKeyframe.m_fTimeIndex = 1.0f;
    Matrix4Scale(kNewKeyframe.m_m4Key, 0.8f, 1.1f, 1.0f);

    kFirstAnim.addKeyframe(kNewKeyframe);

    kNewKeyframe.m_fTimeIndex = 3.0f;
    Matrix4RotationX(kNewKeyframe.m_m4Key, -0.523598f);
    kFirstAnim.addKeyframe(kNewKeyframe);

    kNewKeyframe.m_fTimeIndex = 5.0f;
    Matrix4Identity(kNewKeyframe.m_m4Key);
    kFirstAnim.addKeyframe(kNewKeyframe);

    GameAnimation::AddAnimation(kFirstAnim);


    // - - GAME OBJECT PHASE - - - - - - - - - - //
    akGameObjects = new sh_ptr<GameObject>[GAMECONFIG.m_uiTotalObjects];

    for (unsigned int i = 0; i < GAMECONFIG.m_uiNumObjects; i++)
    {
        akGameObjects[i] =           sh_ptr<GameObject>(new GameObject);
        sh_ptr<Spatial>              objectSpatial(new Spatial);
        sh_ptr<GameObjectController> objectController(new GameObjectController);
        sh_ptr<GameObject>  newChild;

        un_ptr<GameObject> unCurrent(akGameObjects[i]);
        un_ptr<Spatial> unSpatial(objectSpatial);

        // add each leg (3 total)
        for (int j = 0; j < 3; j++)
        {
            un_ptr<GameObject> unCurrentChild = unCurrent;

            newChild = sh_ptr<GameObject>(new GameObject);
            sh_ptr<Spatial> childSpatial(new Spatial);
            un_ptr<Spatial> un_childSpatial(childSpatial);

            Matrix4 trans;
            Matrix4Translation(trans, 0.0f, 0.0f, 1.0f);

            Matrix4 rotY;
            Matrix4RotationY(rotY, 3.14159f/2.0f);

            Matrix4 rot;
            Matrix4RotationZ(rot, (float)j * 2.0f * 3.14159f/3.0f);

            Matrix4Multiply(trans, rotY, rot);

            un_childSpatial->SetTransformation(un_childSpatial, trans);

            sh_ptr<GameMaterial> childMaterial(new GameMaterial);
            un_ptr<GameMaterial> un_childMaterial(childMaterial);

            sh_ptr<GameAnimation> childAnimation(new GameAnimation);

            un_childMaterial->SetDiffuse(un_childMaterial,
                                         0.69f, .120f, 0.24f, 1.0f);

            un_ptr<GameObject> unChild(newChild);

            sh_ptr<GameObjectBound> objectBound =
                sh_ptr<GameObjectBound>(new GameObjectBound());

            un_ptr<GameObjectBound> un_objectBound =
                un_ptr<GameObjectBound>(objectBound);

            un_objectBound->SetBound(un_objectBound, .5f, 0, 0, .5f);


            unChild->InitProperties(unChild,
                                    childSpatial,
                                    childMaterial,
                                    childAnimation,
                                    sh_ptr<GameObjectController>(NULL),
                                    sh_ptr<GamePhysicsProperty>(NULL),
                                    objectBound);

            unCurrentChild->AddChild(unCurrentChild,newChild);

            WORLD.Add(newChild);

            // second part of the leg /////////////////////////////////////////
            newChild = sh_ptr<GameObject>(new GameObject);
            childSpatial = sh_ptr<Spatial> (new Spatial);
            un_childSpatial = un_ptr<Spatial> (childSpatial);

            unCurrentChild = unChild;

            Matrix4Translation(trans, 0.0f, 0.0f, 1.0f);

            un_childSpatial->SetTransformation(un_childSpatial, trans);

            childMaterial = sh_ptr<GameMaterial> (new GameMaterial);
            un_childMaterial = un_ptr<GameMaterial>(childMaterial);

            childAnimation = sh_ptr<GameAnimation> (new GameAnimation);

            un_childMaterial->SetDiffuse(un_childMaterial,
                                         0.69f, .120f, 0.24f, 1.0f);

            objectBound =
                sh_ptr<GameObjectBound>(new GameObjectBound);

            un_objectBound =
                un_ptr<GameObjectBound>(objectBound);

            un_objectBound->SetBound(un_objectBound, .5f, 0, 0, .5f);


            unChild = un_ptr<GameObject>(newChild);
            unChild->InitProperties(unChild,
                                    childSpatial,
                                    childMaterial,
                                    childAnimation,
                                    sh_ptr<GameObjectController>(NULL),
                                    sh_ptr<GamePhysicsProperty>(NULL),
                                    objectBound);
            unCurrentChild->AddChild(unCurrentChild,newChild);

            WORLD.Add(newChild);
        }

        sh_ptr<GamePhysicsProperty> objectPhysics =
            sh_ptr<GamePhysicsProperty>(new GamePhysicsProperty());

        sh_ptr<GameObjectBound> objectBound =
            sh_ptr<GameObjectBound>(new GameObjectBound());

        un_ptr<GameObjectBound> un_objectBound =
            un_ptr<GameObjectBound>(objectBound);

        un_objectBound->SetBound(un_objectBound, .5f, 0, 0, .5f);

        un_ptr<GamePhysicsProperty> un_objectPhysics =
            un_ptr<GamePhysicsProperty>(objectPhysics);

        un_ptr<GameObjectController> un_objectController =
            un_ptr<GameObjectController>(objectController);
        un_objectController->SetState(un_objectController, GameObjectController::GOCS_FLOAT_PHYS);

        un_objectPhysics->SetMass(un_objectPhysics, 1.0f);
        un_objectPhysics->SetVelocity(un_objectPhysics,
                                      ((float)rand()/(float)RAND_MAX) * 0.0f,
                                      ((float)rand()/(float)RAND_MAX) * 0.0f,
                                      ((float)rand()/(float)RAND_MAX) * 0.0f);
        un_objectPhysics->ClearForces(un_objectPhysics);

        sh_ptr<GameMaterial> childMaterial(new GameMaterial);
        un_ptr<GameMaterial> un_childMaterial(childMaterial);
        un_childMaterial->SetDiffuse(un_childMaterial,
                                     0.0f, 0.0f, 0.0f, 1.0f);

        unCurrent->InitProperties(unCurrent,
                                  objectSpatial,
                                  un_childMaterial,
                                  sh_ptr<GameAnimation>(NULL),
                                  objectController,
                                  objectPhysics,
                                  objectBound);

        Matrix4 trans;
        Matrix4Translation(trans,
                           (100.0f * ((float)rand()/(float)RAND_MAX) - 50.0f),
                           (100.0f * ((float)rand()/(float)RAND_MAX) - 50.0f),
                           (100.0f * ((float)rand()/(float)RAND_MAX) - 50.0f));

        Matrix4 scale;
        Matrix4Scale(scale,.5f,.5f,.5f);

        unSpatial->SetTransformation(unSpatial, trans);

        WORLD.Add(akGameObjects[i]);

    }

    // - - EMITER PHASE  - - - - - - - - - - - - //
    for (unsigned int i = 0; i < GAMECONFIG.m_uiNumEmiterObjects; i++)
    {
        akGameObjects[GAMECONFIG.m_uiNumObjects + i] =
            sh_ptr<GameObject>(new GameObject);

        sh_ptr<Spatial> objectSpatial(new Spatial);
        sh_ptr<GameObjectController> objectController(new GameObjectController);
        sh_ptr<GamePhysicsProperty> objectPhysics =
            sh_ptr<GamePhysicsProperty>(new GamePhysicsProperty());

        un_ptr<GameObject> unCurrent(akGameObjects[GAMECONFIG.m_uiNumObjects + i]);

        un_ptr<Spatial> unSpatial(objectSpatial);
        un_ptr<GameObjectController> unController(objectController);
        un_ptr<GamePhysicsProperty> un_objectPhysics =
           un_ptr<GamePhysicsProperty>(objectPhysics);

        unController->SetState(unController,
                               GameObjectController::GOCS_FLOAT);


        un_objectPhysics->SetVelocity(un_objectPhysics,
                                      ((float)rand()/(float)RAND_MAX),
                                      0,
                                      ((float)rand()/(float)RAND_MAX));

        Matrix4 trans;
        Matrix4Translation(trans,
                           (100.0f * ((float)rand()/(float)RAND_MAX)-50.0f),
                           (100.0f * ((float)rand()/(float)RAND_MAX)-50.0f),
                           (100.0f * ((float)rand()/(float)RAND_MAX)-50.0f));


        unSpatial->SetTransformation(unSpatial, trans);

        sh_ptr<GameObjectBound> objectBound =
            sh_ptr<GameObjectBound>(new GameObjectBound());

        un_ptr<GameObjectBound> un_objectBound =
            un_ptr<GameObjectBound>(objectBound);

        un_objectBound->SetBound(un_objectBound, 2.5f, 0, 0, 0.0f);

        sh_ptr<GameMaterial> childMaterial(new GameMaterial);
        un_ptr<GameMaterial> un_childMaterial(childMaterial);

        if (rand()%2 == 0)
        {
         un_objectPhysics->SetMass(un_objectPhysics, -10000.0f);
         un_childMaterial->SetDiffuse(un_childMaterial,
                                     0.0f, 0.5f, 0.2f, 1.0f);
        }
        else
        {
            un_objectPhysics->SetMass(un_objectPhysics, 10000.0f);
            un_childMaterial->SetDiffuse(un_childMaterial,
                                     0.0f, 0.2f, 0.5f, 1.0f);
        }

        unCurrent->InitProperties(unCurrent,
                                  objectSpatial,
                                  childMaterial,
                                  sh_ptr<GameAnimation>(NULL),
                                  objectController,
                                  objectPhysics,
                                  objectBound);

        WORLD.Add(akGameObjects[GAMECONFIG.m_uiNumObjects + i]);

        WORLD.AddGravitationalObject(akGameObjects[GAMECONFIG.m_uiNumObjects + i]);
    }

    // - - THREAD PHASE  - - - - - - - - - - - - //
    if (GAMECONFIG.m_uiNumThreads >= 1)
    {
        akUpdaters.resize(GAMECONFIG.m_uiNumThreads);
        for (unsigned int j = 0; j < GAMECONFIG.m_uiNumThreads; j ++)
        {
            akUpdaters[j] = new GameUpdateThread(j, GAMECONFIG.m_uiNumThreads);
        }

        akThreads.resize(GAMECONFIG.m_uiNumThreads);

        for (unsigned int j = 0; j < GAMECONFIG.m_uiNumThreads; j ++)
        {
            akThreads[j] = new thread(akUpdaters[j]);
        }
    }
    return true;
}

void GameState::Update()
{
    double timeSpent = 0;

    unsigned long long ullStartUpdateTime = RENDER.GrabTimeStamp();

    for (unsigned int uiActionIndex = 0;
        uiActionIndex < GAMECONFIG.m_uiTotalObjects;
        uiActionIndex ++)
    {
        if (GAMESTATE.akGameObjects[uiActionIndex])
        {
            UpdateObject(GAMESTATE.akGameObjects[uiActionIndex], ullStartUpdateTime, 0);
        }
    }

    timeSpent += getNumSecPassed(getElapsedTime(),ullStartUpdateTime);
}

void GameState::Shutdown()
{
    if (GAMECONFIG.m_uiNumThreads >= 1)
    {
        for (unsigned int j = 0; j < akThreads.size(); j++)
        {
            delete akThreads[j];
            akThreads[j] = NULL;
        }
    }

    if (akGameObjects)
    {
       delete[] akGameObjects;
       akGameObjects = NULL;
    }
}
