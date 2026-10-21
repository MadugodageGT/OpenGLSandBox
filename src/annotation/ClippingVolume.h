#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "glm/glm.hpp"

// ClippingVolume.h - CPU-only geometry for the ClippingVolumes annotation method
//
//   F. Ponchio, M. Callieri, M. Dellepiane, R. Scopigno,
//   "Effective Annotations over 3D Models", Computer Graphics Forum, 2019 (Section 6).
//
// Pipeline implemented here (paper Sec. 6.1 / 6.2):
//   1. a closed polyline picked on the surface is projected to view space (window pixels)
//   2. the polyline is triangulated in view space (planar ear clipping)         -> patch A_t (Fig. 8a)
//   3. the surface is rasterised in view space; for every vertex of A_t the
//      minimal front / back offset enclosing the surface chunk is computed      (Fig. 8b)
//   4. wherever a prism still contains a second surface layer (thin features,
//      silhouettes) the triangulation is refined and offsets are recomputed     (Fig. 8c / 8d)
//   5. A_t is extruded along the view rays into a closed, watertight prism      (Fig. 7 right)
//
// Everything is expressed in WINDOW SPACE (x, y in pixels, depth in [0, 1]) so depth values read back
// from the GPU can be used directly. (x, y, depth) is a projective image of world space, therefore
// planar faces stay planar and the depth interpolated by the rasteriser matches the depth interpolated
// here. The finished prism is un-projected to world space as the very last step.
//
// This file has no OpenGL dependency, so it can be unit tested on its own.

namespace clipvol {

// A pixel shows a surface if its depth is below the clear value (1.0).
inline bool IsSurface(float depth) { return depth < 1.0f; }

struct DepthMap
{
    int width = 0;
    int height = 0;
    std::vector<float> data;   // window depth, row 0 = bottom row (GL convention), 1.0 = background

    bool Valid() const
    {
        return width > 0 && height > 0 &&
               data.size() == static_cast<size_t>(width) * static_cast<size_t>(height);
    }
    float At(int x, int y) const { return data[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)]; }
};

struct BuildParams
{
    float depthMargin = 2e-5f;       // padding added in front of / behind the surface, in window-depth units
    bool  refineThinFeatures = true; // paper Fig. 8c/8d: subdivide prisms that still contain a second surface
    int   maxRefineRounds = 8;
    float minTriangleAreaPx = 40.0f; // triangles smaller than this (px^2) are never subdivided
    int   maxTriangles = 6000;       // safety cap for the refined 2D triangulation
};

struct BuildInput
{
    std::vector<glm::vec2> polygonPx;  // outline in window pixels (y up), open or closed
    const DepthMap* layer1 = nullptr;  // nearest surface layer (same size as the viewport)
    const DepthMap* layer2 = nullptr;  // second layer (depth peeled); optional, needed for refinement
    glm::mat4 viewProj = glm::mat4(1.0f);
};

struct BuildResult
{
    bool ok = false;
    std::string error;

    // Closed, consistently oriented (CCW seen from outside) triangle mesh in world space.
    // Vertices [0, N) are the front cap (towards the camera), [N, 2N) the back cap.
    std::vector<glm::vec3> positions;
    std::vector<uint32_t>  indices;

    // Intermediate window-space data (kept for statistics and tests).
    std::vector<glm::vec2> px;       // refined 2D vertices (window pixels)
    std::vector<uint32_t>  tris2D;   // refined 2D triangulation (CCW)
    std::vector<float>     front;    // front depth per 2D vertex
    std::vector<float>     back;     // back depth per 2D vertex
    int refineRounds = 0;
};

// Builds the closed clipping prism for an outline. See the file comment for the steps.
BuildResult BuildClippingVolume(const BuildInput& in, const BuildParams& params);

// Planar ear-clipping triangulation of a simple polygon (any winding). Output triangles are CCW and
// index into outVerts. Fails for self-intersecting or degenerate outlines.
bool TriangulatePolygon(std::vector<glm::vec2> poly,
                        std::vector<glm::vec2>& outVerts,
                        std::vector<uint32_t>& outTris,
                        std::string& error);

// Approximate area of the model surface enclosed by the volume (paper Sec. 6.2: numeric data computed
// at annotation time). A mesh triangle counts when its centroid lies inside the prism.
// Mesh positions must be in the same space the view-projection matrix expects.
float ApproximateEnclosedArea(const BuildResult& volume,
                              const glm::mat4& viewProj,
                              int width, int height,
                              const std::vector<glm::vec3>& meshPositions,
                              const std::vector<uint32_t>& meshIndices);

// Mesh sanity helpers (also used by the tests).
float SignedVolume(const std::vector<glm::vec3>& positions, const std::vector<uint32_t>& indices);
bool  CheckWatertight(const std::vector<uint32_t>& indices, std::string* why = nullptr);

} // namespace clipvol
