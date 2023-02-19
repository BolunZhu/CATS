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

#include "Renderer.hpp"
#include "RenderThread.hpp"

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

Renderer RENDER;

GLfloat light_diffuse[]  = {1.0, 1.0, 1.0, 1.0};
GLfloat light_specular[] = {1.0, 1.0, 1.0, 1.0};
GLfloat light_ambient[]  = {0.0, 0.0, 0.0, 1.0};
GLfloat light_position[] = {1.0, 1.0, -1.0, 0.0};

GLfloat n[6][3] = {  /* Normals for the 6 faces of a cube. */
    {-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0},
    {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0} };
GLint faces[6][4] = {  /* Vertex indexes for the 6 faces of a cube. */
    {0, 1, 2, 3}, {3, 2, 6, 7}, {7, 6, 5, 4},
    {4, 5, 1, 0}, {5, 6, 2, 1}, {7, 4, 0, 3} };

GLfloat v[8][3];  /* Will be filled in with X,Y,Z vertexes. */


GLfloat mat_Kd[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat mat_Ka[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
GLfloat mat_Ks[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
GLfloat mat_Ke[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

void Renderer::DrawText(float x, float y, void* font,
                        const char* c_str)
{
    int len = (int)strlen(c_str);
    if (len <= 0)
        return;

    glRasterPos2f(x, y);

    for (int i = 0; i < len; i++)
    {
        glutBitmapCharacter(font, c_str[i]);
    }
}

void Renderer::Render(void)
{

    // ------ Render Thread Shutdown Check ------------------------------ //
    // Because glut takes over the render thread, we must check here for
    // game shutdown in order to run stm::shutdown on this thread.
    if (gIsGameOver())
    {
        // Once app has been shut down, disable transactions for this
        // thread (and force output). m_bRenderIsShutdown flag makes sure
        // we only shutdown once.
        if (!m_bRenderIsShutdown)
        {
            stm::shutdown(1000);
            m_bRenderIsShutdown = true;

            // since we're stuck in the glut main loop, which never returns on
            // its own, we'll manually exit
#ifndef _MSC_VER
            pthread_exit(NULL);
#else
            ExitThread(0);
#endif

        }
        return;
    }


    // ------ Set Renderer to Screen Space ------------------------------ //
    //  This mode allows for rendering directly to pixels on the screen
    //  without perspective transformation.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,
               GAMECONFIG.m_uiWindowWidth,
               0,
               GAMECONFIG.m_uiWindowHeight);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();


    // ------ Render Background ----------------------------------------- //
    //  We want a nice smooth background gradient, unless the user
    //  decides otherwise, in which case we just do a normal clear of the
    //  color bits.
    glClear(GL_DEPTH_BUFFER_BIT);

    if (GAMECONFIG.m_bRenderGrad)
    {
        glBegin(GL_QUADS);

        glColor4ub(191,235,248,255);
        glVertex2f(0,0);
        glVertex2f(GAMECONFIG.m_uiWindowWidth,0);

        glColor4ub(92,183,219,255);
        glVertex2f(GAMECONFIG.m_uiWindowWidth,GAMECONFIG.m_uiWindowHeight);
        glVertex2f(0,GAMECONFIG.m_uiWindowHeight);

        glEnd();

        glClear(GL_DEPTH_BUFFER_BIT);
        glColor3ub(0,0,0);
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3ub(255,255,255);
    }

    // ------ Grab Time Stamp ------------------------------------------- //
    unsigned long long ullFrameTimeStamp = getElapsedTime();
    SetTimeStamp(ullFrameTimeStamp);


    // ------ Render HUD (Head's Up Display) ---------------------------- //
    //  If the user desires, we will output frame and update rate info to
    //  the screen.

    if (GAMECONFIG.m_bDrawHUD)
    {
        char c_str[200];

        // Generate the FPS text and print it.
        sprintf_s(c_str, "F:%8.2f",
                PROF.GetLocalEventRate(Profiler::PROF_FRAME_RENDER,
                                       GAMECONFIG.m_uiNumThreads,
                                       ullFrameTimeStamp));

         DrawText(10, 10, GLUT_BITMAP_HELVETICA_10, c_str);

         // Now print each updater thread's commits per second.
         for (unsigned int i = 0; i < GAMECONFIG.m_uiNumThreads; i++)
         {
             float ups = PROF.GetLocalEventRate(Profiler::PROF_BATCH_UPDATE,
                                               i,
                                               ullFrameTimeStamp);

            sprintf_s(c_str, "%d UPS:  %8.2f", i, ups);
            if (ups < 2.0f)
            {
                glColor3ub(0,155,0);
            }

            DrawText(10, 25 + 15 * i, GLUT_BITMAP_HELVETICA_10, c_str);
            glColor3ub(0,0,0);
        }
    }


    // ------ Projection Mode ------------------------------------------- //
    //  Turn on perspective mode and place the camera.
    SetProjectionMode(true);

    v3CameraPosition[0] = 0;
    v3CameraPosition[1] =  m_fZoom;
    v3CameraPosition[2] = -m_fZoom;
    gluLookAt(v3CameraPosition[0],
              v3CameraPosition[1],
              v3CameraPosition[2],
              0.0, 0.0, 0.0,
              0.0, 1.0, 0.0);


    // -- Iterate the game state ---------------------------------------- //
    //  We have 2 render pipelines, one that uses inev to draw n visual
    //  objects inside one transaction.
    //
    //  One that uses a large buffer to draw n visual objects inside one
    //  transaction.


    // If the gamestate has been inited
    if (GAMESTATE.akGameObjects)
    {
        // Render visual objects in batches. If inevitability isn't
        // available, we use an arbitrarily large buffer to leak
        // the objects. Note that there are GAMECONFIG.m_uiNumObjects
        // tripod objects.

        for (unsigned int j = 0; j < GAMECONFIG.m_uiNumObjects; j += GAMECONFIG.m_uiRenderBatchSize)
        {
            // If no INEV, we will fill this buffer with copies of
            // the data we want to render.
            std::vector<GameObject::RenderableSegment> voExtractBuffer;

            BEGIN_TRANSACTION

            bool bIsInev = false;
            if (!GAMECONFIG.m_bNoRenderInev)
                bIsInev = stm::try_inevitable();

            for (unsigned int i = 0; i < GAMECONFIG.m_uiRenderBatchSize &&
                                     j+i < GAMECONFIG.m_uiNumObjects; i++)
            {
                rd_ptr<GameObject> rdoCurrent = rd_ptr<GameObject>(GAMESTATE.akGameObjects[i+j]);

                if (!bIsInev)
                {
                    rdoCurrent->ExtractRender(rdoCurrent, voExtractBuffer);
                }
                else
                {
                    rdoCurrent->Render(rdoCurrent);
                }
            }

            END_TRANSACTION
            PROF.LogEvent(Profiler::PROF_COMMIT_RENDER,GAMECONFIG.m_uiNumThreads);

            for (unsigned int i = 0; i < voExtractBuffer.size(); i++)
            {
                GameObject::RenderSegment(voExtractBuffer[i]);
            }
        }


        // -------------------------------------------------------------- //
        // Now render all remaining objects. (PLANETS)

        // DRAW ALL PLANETS AT ONCE
        if (GAMECONFIG.m_uiRenderBatchSize > 1)
        {
            // If no INEV, we will fill this buffer with copies of
            // the data we want to render.
            std::vector<GameObject::RenderableSegment> voExtractBuffer;

            BEGIN_TRANSACTION
            bool bIsInev = false;

            if (!GAMECONFIG.m_bNoRenderInev)
                bIsInev = stm::try_inevitable();

            for (unsigned int j = GAMECONFIG.m_uiNumObjects; j < GAMECONFIG.m_uiTotalObjects; j ++)
            {
                rd_ptr<GameObject> rdoCurrent = rd_ptr<GameObject>(GAMESTATE.akGameObjects[j]);

                if (!bIsInev)
                {
                    rdoCurrent->ExtractRender(rdoCurrent, voExtractBuffer);
                }
                else
                {
                    rdoCurrent->Render(rdoCurrent);
                }
            }
            END_TRANSACTION
            PROF.LogEvent(Profiler::PROF_COMMIT_RENDER,GAMECONFIG.m_uiNumThreads);

            for (unsigned int i = 0; i < voExtractBuffer.size(); i++)
            {
                GameObject::RenderSegment(voExtractBuffer[i]);
            }
        }
        else // DRAW PLANETS ONE AT A TIME
        {
            for (unsigned int j = GAMECONFIG.m_uiNumObjects;
                 j < GAMECONFIG.m_uiTotalObjects; j ++)
            {
                // If no INEV, we will fill this buffer with copies of
                // the data we want to render.
                std::vector<GameObject::RenderableSegment> voExtractBuffer;

                BEGIN_TRANSACTION
                bool bIsInev = false;

                if (!GAMECONFIG.m_bNoRenderInev)
                    bIsInev = stm::try_inevitable();

                rd_ptr<GameObject> rdoCurrent = rd_ptr<GameObject>(GAMESTATE.akGameObjects[j]);

                if (!bIsInev)
                {
                    rdoCurrent->ExtractRender(rdoCurrent, voExtractBuffer);
                }
                else
                {
                    rdoCurrent->Render(rdoCurrent);
                }
                END_TRANSACTION
                PROF.LogEvent(Profiler::PROF_COMMIT_RENDER,GAMECONFIG.m_uiNumThreads);

                for (unsigned int i = 0; i < voExtractBuffer.size(); i++)
                {
                    GameObject::RenderSegment(voExtractBuffer[i]);
                }
            }
        }
    }

    // Present the frame and if vsync lock is in wait till the next sync opportunity.
    glutSwapBuffers();
    PROF.LogEvent(Profiler::PROF_FRAME_RENDER,GAMECONFIG.m_uiNumThreads);
}

unsigned long long Renderer::GrabTimeStamp()
{
   // unsigned long long ullReturnVal;
  //  mvx(&ullTimeStamp,&ullReturnVal);
    return getElapsedTime();//ullReturnVal;
}

void Renderer::SetTimeStamp(unsigned long long ullNewTimeStamp)
{
    mvx(&ullNewTimeStamp,&ullTimeStamp);
}


void Renderer::Resize(int iWidth, int iHeight)
{
    GAMECONFIG.m_uiWindowWidth  = iWidth;
    GAMECONFIG.m_uiWindowHeight = iHeight;

    // Set the viewport to be the entire window
    glViewport(0, 0, iWidth, iHeight);
}

void Renderer::GetCameraPosition(float& x, float& y, float&z)
{
    x = v3CameraPosition[0];
    y = v3CameraPosition[1];
    z = v3CameraPosition[2];
}

void Renderer::DrawBound(const Vector3 vec3Corner,
                         float width)
{
    if (!GAMECONFIG.m_bDrawBounds || width < .00001f)
        return;

    bool sphere = true;
    glDepthMask(false);

    glPushMatrix();

    glTranslatef(vec3Corner[0],
                 vec3Corner[1],
                 vec3Corner[2]);

    if (sphere)
    {
        glColor4f(0.69f, .120f, 0.24f,width/20.0f);
        glutSolidSphere(width,15,10);
    }
    else
    {
        glColor4f(1, 1, 1,width/10.0f);
        glutSolidCube(width*2.0f);
    }

    glPopMatrix();
    glDepthMask(true);
}

void Renderer::DrawLine(float v3Start[],
                        float v3End[])
{
    glLineWidth(5.0f);
    glBegin(GL_LINES);
    glVertex3fv(v3Start);
    glVertex3fv(v3End);
    glEnd();

    glLineWidth(1.0f);
}

void Renderer::DrawMesh(const float transform[],
                        const float material[], int type)
{
    if (m_bWireframe)
    {
        glPolygonMode(GL_FRONT, GL_LINE);
    }

    glPushMatrix();

    glMultMatrixf(transform);
    glColor4fv(material);

    glCallList(type);

    glPopMatrix();

    glPolygonMode(GL_FRONT, GL_FILL);
}

void Renderer::SetProjectionMode(bool bPerspective)
{
    float ratio = (float)GAMECONFIG.m_uiWindowWidth/
                  (float)GAMECONFIG.m_uiWindowHeight;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (bPerspective)
        gluPerspective(60, ratio, 1, 256);
    else
        glOrtho(-ratio * 10.0f, ratio * 10.0f, -10.0f, 10.0f, 1, 256);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

bool Renderer::Init(int argc, char* argv[])
{
    m_bRenderIsShutdown = false;

    m_pkRenderThread = new RenderThread(argc, argv);
    m_pkThread = new thread(m_pkRenderThread);

    return true;
}

void Renderer::ToggleWireframe()
{
    m_bWireframe = !m_bWireframe;
}

void Renderer::Shutdown()
{
    if (m_pkThread)
        delete m_pkThread;

    m_pkThread = NULL;
}

void Renderer::ConfigureRenderer()
{
    /* Setup cube vertex data. */
    v[0][0] = v[1][0] = v[2][0] = v[3][0] = -.125f;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] = .125f;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = -.125f;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] = .125f;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = 1;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] = 0;

    GLfloat light_pos[4] = { 1.0f, 1.0, 1.0, 0.0 };

    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

     glEnable(GL_LIGHTING);

    glLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER, 1.0f);
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1.0f);

    glEnable(GL_LIGHT0);

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  mat_Ka);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  mat_Kd);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_Ks);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mat_Ke);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 128.0f);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);


    /* Use depth buffering for hidden surface elimination. */
    glEnable(GL_DEPTH_TEST);


    if (GAMECONFIG.m_bUseAA)
    {
        // AA is ON!
        glHint(GL_POINT_SMOOTH, GL_NICEST);
        glHint(GL_LINE_SMOOTH, GL_NICEST);
        glHint(GL_POLYGON_SMOOTH, GL_NICEST);

        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_POLYGON_SMOOTH);
        glEnable(GL_POINT_SMOOTH);

        glLineWidth(.5f);
    }

    if (GAMECONFIG.m_bUseAlpha || GAMECONFIG.m_bUseAA)
    {
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    }


    bool octoBox = !GAMECONFIG.m_bRenderBox;
    // create the render object display list:
    glNewList(1, GL_COMPILE);
    if (octoBox)
    {
        glBegin(GL_LINES);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, 1.0f);

        glVertex3f(-0.125f, 0.0f, 0.0f);
        glVertex3f(0.125f, 0.0f, 0.0f);
        glVertex3f(0.0f, -0.125f, 0.0f);
        glVertex3f(0.0f, 0.125f, 0.0f);

        glEnd();
    }
    else
    {

        glEnable(GL_LIGHTING);
        for (unsigned int i = 0; i < 6; i++) {
            glBegin(GL_QUADS);
            glNormal3fv(&n[i][0]);
            glVertex3fv(&v[faces[i][3]][0]);
            glVertex3fv(&v[faces[i][2]][0]);
            glVertex3fv(&v[faces[i][1]][0]);
            glVertex3fv(&v[faces[i][0]][0]);
            glEnd();
        }
        glDisable(GL_LIGHTING);
    }

    glEndList();

    glNewList(2, GL_COMPILE);
    glEnable(GL_LIGHTING);
    glutSolidSphere(2.5f,20,15);
    glDisable(GL_LIGHTING);
    glEndList();

    m_bWireframe = false;

    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glClearColor(.0f, .02f, .16f, 1.0f);
}
