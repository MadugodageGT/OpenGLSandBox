#pragma once

#include <GL/glew.h>

// GLStateGuard - saves the OpenGL state the annotation passes modify and restores it on destruction,
// so the passes can freely change depth/stencil/blend/framebuffer state without disturbing the other
// tests in the sandbox (which assume the defaults set up in Application.cpp).
//
// Stencil function / operations are restored to the GL defaults (ALWAYS, KEEP) because nothing else in
// the sandbox uses the stencil buffer.

class GLStateGuard
{
public:
    GLStateGuard()
    {
        glGetIntegerv(GL_VIEWPORT, m_Viewport);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_DrawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_ReadFbo);
        glGetIntegerv(GL_DEPTH_FUNC, &m_DepthFunc);
        glGetIntegerv(GL_STENCIL_WRITEMASK, &m_StencilMask);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &m_DepthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, m_ColorMask);

        m_DepthTest = glIsEnabled(GL_DEPTH_TEST);
        m_DepthClamp = glIsEnabled(GL_DEPTH_CLAMP);
        m_Blend = glIsEnabled(GL_BLEND);
        m_StencilTest = glIsEnabled(GL_STENCIL_TEST);
        m_CullFace = glIsEnabled(GL_CULL_FACE);
        m_ProgramPointSize = glIsEnabled(GL_PROGRAM_POINT_SIZE);
    }

    ~GLStateGuard()
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_DrawFbo));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_ReadFbo));
        glViewport(m_Viewport[0], m_Viewport[1], m_Viewport[2], m_Viewport[3]);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDepthFunc(static_cast<GLenum>(m_DepthFunc));
        glDepthMask(m_DepthMask);
        glColorMask(m_ColorMask[0], m_ColorMask[1], m_ColorMask[2], m_ColorMask[3]);

        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(static_cast<GLuint>(m_StencilMask));

        Set(GL_DEPTH_TEST, m_DepthTest);
        Set(GL_DEPTH_CLAMP, m_DepthClamp);
        Set(GL_BLEND, m_Blend);
        Set(GL_STENCIL_TEST, m_StencilTest);
        Set(GL_CULL_FACE, m_CullFace);
        Set(GL_PROGRAM_POINT_SIZE, m_ProgramPointSize);
    }

    GLStateGuard(const GLStateGuard&) = delete;
    GLStateGuard& operator=(const GLStateGuard&) = delete;

private:
    static void Set(GLenum cap, GLboolean on)
    {
        if (on)
            glEnable(cap);
        else
            glDisable(cap);
    }

    GLint m_Viewport[4] = { 0, 0, 0, 0 };
    GLint m_DrawFbo = 0;
    GLint m_ReadFbo = 0;
    GLint m_DepthFunc = GL_LESS;
    GLint m_StencilMask = 0xFF;
    GLboolean m_DepthMask = GL_TRUE;
    GLboolean m_ColorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };

    GLboolean m_DepthTest = GL_FALSE;
    GLboolean m_DepthClamp = GL_FALSE;
    GLboolean m_Blend = GL_FALSE;
    GLboolean m_StencilTest = GL_FALSE;
    GLboolean m_CullFace = GL_FALSE;
    GLboolean m_ProgramPointSize = GL_FALSE;
};
