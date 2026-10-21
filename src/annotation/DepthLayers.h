#pragma once

#include <GL/glew.h>

#include "glm/glm.hpp"

#include "Shader.h"
#include "annotation/ClippingVolume.h"

class SurfaceMesh;

// DepthLayers - rasterises a mesh in view space and reads the depth back to the CPU (paper Sec. 6.2:
// "we rasterize in view space the real surface S").
//
//   layer1: depth of the nearest surface at every pixel
//   layer2: depth of the SECOND surface (depth peeling), used to detect thin features / back surfaces
//           that would end up inside a clipping prism
//
// Depth values are window-space depth in [0, 1]; background pixels hold 1.0. Row 0 is the bottom row.
// Render targets are created lazily and resized to match the requested size.

class DepthLayers
{
public:
    DepthLayers();
    ~DepthLayers();

    DepthLayers(const DepthLayers&) = delete;
    DepthLayers& operator=(const DepthLayers&) = delete;

    // `mvp` must be the same matrix the model is drawn with. Restores the GL state it touches.
    bool Capture(const SurfaceMesh& mesh, const glm::mat4& mvp, int width, int height,
                 clipvol::DepthMap& layer1, clipvol::DepthMap& layer2);

private:
    void Resize(int width, int height);

    Shader m_Shader;
    GLuint m_Fbo;
    GLuint m_Depth[2];
    int m_Width;
    int m_Height;
};
