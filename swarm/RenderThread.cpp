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

#include "RenderThread.hpp"

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif


// -- Global Variable(s) ------------------------------------------------ //
sh_ptr<GameObject> g_shGrab(NULL); // store whatever object we are holding.

int g_iMouseX = 0; // hold the mouse x value
int g_iMouseY = 0; // hold the mouse y value


// -- gRender ----------------------------------------------------------- //
//  Callback for GLUT to be called whenever a frame must be rendered.
//  Pass the call on to the RENDER singleton.
void gRender()
{
    RENDER.Render();
}

// -- gKeyboard --------------------------------------------------------- //
//  Callback for GLUT on keyboard input. When our game window has the
//  focus of the keyboard, and a key is pressed, this function is called
//  with key being the key pressed and x, y being the mouse position in
//  pixels.
//
//  Key Options:
//     h  : Toggle HUD (Heads Up Display).
//     q  : Zoom in.
//     a  : Zoom out.
//     w  : Toggle wireframe.
//     b  : Toggle bounding sphere rendering. (Big Performance Hit)
//     c  : Toggle collision detection.
void gKeyboard(unsigned char key, int x, int y)
{
    g_iMouseX = x;
    g_iMouseY = y;

    if (!g_bIsAppAlive)
    {
        return;
    }

    if (key == 27)
    {
        // quit on esc key
        gKillGame();
    }
    else if (key == 'h')
    {
        GAMECONFIG.m_bDrawHUD = !GAMECONFIG.m_bDrawHUD;
    }
    else if (key == 'q')
    {
        float fNewZoom = RENDER.GetZoom() - 2.5f;
        if (fNewZoom < 1.0f)
        {
            fNewZoom = 1.0f;
        }
        RENDER.SetZoom(fNewZoom);
    }
    else if (key == 'a')
    {
        RENDER.SetZoom(RENDER.GetZoom() + 2.5f);
    }
    else if (key == 'w')
    {
        RENDER.ToggleWireframe();
    }
    else if (key == 'b')
    {
        GAMECONFIG.m_bDrawBounds = !GAMECONFIG.m_bDrawBounds;
    }
}

// -- gChangeSize ------------------------------------------------------- //
//  Glut callback for when window is resized. Pass info to RENDER
//  singleton.
void gChangeSize(int w, int h)
{
    RENDER.Resize(w, h);
}


// -- gIdle ------------------------------------------------------------- //
//  Glut callback for when thread is idle. We may have to update the
//  position of a held object because the camera moved, do so here.
//  also push renders by calling glutPostRedisplay.
void gIdle(void)
{
    if (!glutGetWindow())
    {
        gKillGame();
    }

    if (GAMECONFIG.m_uiNumThreads == 0)
    {
        GAMESTATE.Update();
    }

    glutPostRedisplay();
}


void RenderThread::operator()() {

    RENDER.SetTimeStamp(getElapsedTime());

    // -- Start the Clock ------------------------------------------- //
    g_ullAppStartTime = RENDER.GrabTimeStamp();
    PROF.StartProfiling(g_ullAppStartTime);


    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(GAMECONFIG.m_uiWindowWidth,
                       GAMECONFIG.m_uiWindowHeight);

    glutInit(&m_argc, m_argv);

    if (!GAMECONFIG.m_bWindowedMode)
    {
        char temp[64];
        sprintf_s(temp,
                "%dx%d:%d@%d",
                GAMECONFIG.m_uiWindowWidth,
                GAMECONFIG.m_uiWindowHeight,
                32,
                75);

        glutGameModeString(temp);

        // start fullscreen game mode
        glutEnterGameMode();
    }
    else
    {
        glutCreateWindow("Swarm: Rise of the Transactional Tripods");
    }

    atexit(gKillGame);
    glutKeyboardFunc(gKeyboard);
    glutDisplayFunc(gRender);
    glutReshapeFunc(gChangeSize);
    glutIdleFunc(gIdle);

    // -- VSync ----------------------------------------------------- //
    // Note that OpenGL by default blocks
    // rendering to match the actual monitor refresh rate
    //
    // We want to push frames ASAP, perhaps starting a new one
    // before the old one is presented to the monitor. This allows
    // for FPS of thousands on very simple apps.
    //
    // I do not know the linux equivalent of this code
#ifdef _MSC_VER
    if (!GAMECONFIG.m_bVSyncLock)
    {
        typedef bool (APIENTRY *PFNWGLSWAPINTERVALFARPROC)(int);
        PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;

        wglSwapIntervalEXT =
            (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress("wglSwapIntervalEXT");

        if (wglSwapIntervalEXT)
            wglSwapIntervalEXT(0);
    }
#endif


    // -- Set GL for RENDER ----------------------------------------- //
    RENDER.ConfigureRenderer();

    // -- Give GLUT control of this thread forever ------------------ //
    glutMainLoop();
}
