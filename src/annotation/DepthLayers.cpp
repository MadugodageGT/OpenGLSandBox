#include "DepthLayers.h"

#include "Renderer.h"
#include "annotation/GLStateGuard.h"
#include "annotation/SurfaceMesh.h"

DepthLayers::DepthLayers()
    : m_Shader("res/shaders/ClipDepth.shader"), m_Fbo(0), m_Width(0), m_Height(0)
{
    m_Depth[0] = 0;
    m_Depth[1] = 0;
}

DepthLayers::~DepthLayers()
{
    if (m_Depth[0]) glDeleteTextures(2, m_Depth);
    if (m_Fbo) glDeleteFramebuffers(1, &m_Fbo);
}

void DepthLayers::Resize(int width, int height)
{
    if (m_Fbo && width == m_Width && height == m_Height)
        return;

    if (!m_Fbo)
    {
        glGenFramebuffers(1, &m_Fbo);
        glGenTextures(2, m_Depth);
    }

    for (int i = 0; i < 2; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, m_Depth[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    m_Width = width;
    m_Height = height;
}

bool DepthLayers::Capture(const SurfaceMesh& mesh, const glm::mat4& mvp, int width, int height,
                          clipvol::DepthMap& layer1, clipvol::DepthMap& layer2)
{
    if (width <= 0 || height <= 0)
        return false;

    GLStateGuard guard;   // restores framebuffer, viewport and the enables/masks changed below

    Resize(width, height);

    GLCall(glBindFramebuffer(GL_FRAMEBUFFER, m_Fbo));
    GLCall(glViewport(0, 0, width, height));
    GLCall(glDisable(GL_BLEND));
    GLCall(glDisable(GL_STENCIL_TEST));
    GLCall(glDisable(GL_CULL_FACE));
    GLCall(glDisable(GL_DEPTH_CLAMP));
    GLCall(glEnable(GL_DEPTH_TEST));
    GLCall(glDepthFunc(GL_LESS));
    GLCall(glDepthMask(GL_TRUE));
    GLCall(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

    m_Shader.Bind();
    m_Shader.setUniformMat4f("u_MVP", mvp);
    m_Shader.setUniform1f("u_PeelEps", 1e-6f);
    m_Shader.setUniform1i("u_PrevDepth", 0);

    bool ok = true;
    for (int layer = 0; layer < 2 && ok; ++layer)
    {
        GLCall(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_Depth[layer], 0));
        GLCall(glDrawBuffer(GL_NONE));
        GLCall(glReadBuffer(GL_NONE));

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            ok = false;
            break;
        }

        GLCall(glClearDepth(1.0));
        GLCall(glClear(GL_DEPTH_BUFFER_BIT));

        // Sample the other layer's texture: for layer 1 that is the nearest layer (not attached right now);
        // for layer 0 it is a texture that the shader never reads. Never the attached one (no feedback loop).
        GLCall(glActiveTexture(GL_TEXTURE0));
        GLCall(glBindTexture(GL_TEXTURE_2D, m_Depth[1 - layer]));
        m_Shader.setUniform1i("u_Peel", layer);

        mesh.Draw();
    }

    if (ok)
    {
        clipvol::DepthMap* out[2] = { &layer1, &layer2 };
        GLCall(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        for (int layer = 0; layer < 2; ++layer)
        {
            out[layer]->width = width;
            out[layer]->height = height;
            out[layer]->data.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 1.0f);
            GLCall(glBindTexture(GL_TEXTURE_2D, m_Depth[layer]));
            GLCall(glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, out[layer]->data.data()));
        }
    }

    GLCall(glBindTexture(GL_TEXTURE_2D, 0));
    return ok;
}
