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
//  Renderer.hpp -                                                        //
//    Interacts with OpenGL to draw transactionally managed game objects. //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "Game.hpp"

class RenderThread;
class thread;

class Renderer
{
  public:

    Renderer() : m_fZoom(50.0f) { SetTimeStamp(getElapsedTime()); };

    // -- Renderer Control Functions------------------------------------- //

    bool Init(int argc, char* argv[]);

    void ConfigureRenderer(); // Set OpenGL properties. Must be called in
                              // the thread that contains the window
                              // context spawned by glut.

    void Shutdown();          // Clean up renderer.

    void Render();            // Draw a frame, called by GLUT.
    void Resize(int iWidth,   // Window resize, balled by GLUT.
                int iHeight);

    // -- Renderer Draw Functions ---------------------------------------- //
    //  Call inside the render loop to draw various items.

    void DrawMesh(const float transform[],  // Sphere or box.
                  const float material[],
                  int type = 1);

    void DrawLine(float v3Start[],
                  float v3End[]);

    void DrawBound(const Vector3 vec3Corner, // bounding sphere
                   float width);


    void SetZoom(float fZ){ m_fZoom = fZ; };
    float GetZoom(){ return m_fZoom; };

    void ToggleWireframe();

    void GetCameraPosition(float& x, float& y, float&z);

    // Return the current global time stamp.
    unsigned long long GrabTimeStamp();

    // Set the current global time stamp.
    void SetTimeStamp(unsigned long long);

    int m_iWindowId;
  private:

    void DrawText(float x, float y, void *font, const char *c_str);

    void SetProjectionMode(bool bPerspective);

    Vector3 v3CameraPosition;
    float m_fZoom;

    volatile bool m_bRenderIsShutdown;
    bool m_bWireframe;

    volatile unsigned long long ullTimeStamp;

    // Thread that will loop forever with glut main.
    RenderThread * m_pkRenderThread;
    thread * m_pkThread;

  public:
    // Getter for the m_bRenderIsShutdown.  Using this from the main thread
    // should prevent a race that lets the program exit before the renderer has
    // called stm::shutdown()
    bool isRendererShutdown()
    {
        return m_bRenderIsShutdown;
    }

};

void glutRender();

extern Renderer RENDER;

#endif // __RENDERER_H__
