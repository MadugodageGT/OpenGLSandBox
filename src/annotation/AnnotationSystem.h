#pragma once

#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>

#include "glm/glm.hpp"

#include "Shader.h"
#include "annotation/ClippingVolume.h"
#include "annotation/DepthLayers.h"

class SurfaceMesh;

// A closed clipping prism on the GPU (positions + indices). Never drawn on its own in the final image:
// it is only used to fill the stencil buffer (see AnnotationSystem::RenderOverlays).
class VolumeMesh
{
public:
    explicit VolumeMesh(const clipvol::BuildResult& built);
    ~VolumeMesh();

    VolumeMesh(const VolumeMesh&) = delete;
    VolumeMesh& operator=(const VolumeMesh&) = delete;

    void Draw() const;
    size_t TriangleCount() const { return static_cast<size_t>(m_IndexCount) / 3; }
    int RefineRounds() const { return m_RefineRounds; }

private:
    GLuint m_Vao;
    GLuint m_Vbo;
    GLuint m_Ebo;
    GLsizei m_IndexCount;
    int m_RefineRounds;
};

// An annotation = one or more clipping volumes sharing a colour. Several volumes are how an annotation
// that cannot be seen from a single view is built (paper "Complex annotations"): the union of the
// volumes comes for free from the stencil test (inside any volume => stencil != 0).
struct Annotation
{
    int id = 0;
    std::string name;
    glm::vec4 color = glm::vec4(1.0f);
    bool visible = true;
    std::vector<std::unique_ptr<VolumeMesh>> volumes;

    float perimeter = 0.0f;   // length of the traced outline(s), in model units
    float area = 0.0f;        // approximate enclosed surface area, in model units^2

    size_t TriangleCount() const
    {
        size_t n = 0;
        for (size_t i = 0; i < volumes.size(); ++i)
            n += volumes[i]->TriangleCount();
        return n;
    }
};

struct OverlaySettings
{
    bool zFail = true;     // true: depth-fail stencil counting (robust when the camera is inside a volume)
    float alpha = 0.55f;   // opacity of the annotation tint
};

// AnnotationSystem owns the annotations and implements the two halves of the method:
//   creation  - CreateFromPolyline(): depth capture + clipping volume construction (CPU, ClippingVolume.cpp)
//   display   - RenderOverlays():     two-pass stencil rendering of every annotation (paper Sec. 6.3)
//
// All GL calls must happen on the thread that owns the context.
class AnnotationSystem
{
public:
    AnnotationSystem();
    ~AnnotationSystem();

    AnnotationSystem(const AnnotationSystem&) = delete;
    AnnotationSystem& operator=(const AnnotationSystem&) = delete;

    struct CreateResult
    {
        bool ok = false;
        std::string message;
        int annotationId = -1;
    };

    // Reads the depth under window pixel (px, py) (origin bottom-left) from the bound read framebuffer and
    // un-projects it. Call after the model has been drawn. Returns false over the background.
    bool PickSurfacePoint(int px, int py, const glm::mat4& viewProj, int viewportW, int viewportH,
                          glm::vec3& outWorld) const;

    // Builds a clipping volume from an outline picked on the surface, as seen from `viewProj`, and adds
    // it to annotation `targetAnnotation` (or to a new annotation when negative / unknown).
    CreateResult CreateFromPolyline(const SurfaceMesh& mesh,
                                    const std::vector<glm::vec3>& polylineWorld,
                                    const glm::mat4& viewProj, int viewportW, int viewportH,
                                    int targetAnnotation,
                                    const clipvol::BuildParams& params);

    // Plain lit draw of the model (alpha 1) - also used by the tint pass.
    void DrawMesh(const SurfaceMesh& mesh, const glm::mat4& viewProj, const glm::vec3& camPos,
                  const glm::vec3& color, float alpha);

    // Two-pass stencil rendering of all visible annotations. The model must already be in the depth buffer.
    void RenderOverlays(const SurfaceMesh& mesh, const glm::mat4& viewProj, const glm::vec3& camPos,
                        const OverlaySettings& settings);

    // Debug views (the clipping prisms are not rendered in the real annotation system).
    void RenderVolumeWireframes(const glm::mat4& viewProj);
    void RenderPolyline(const std::vector<glm::vec3>& points, const glm::mat4& viewProj);

    std::vector<std::unique_ptr<Annotation>>& Annotations() { return m_Annotations; }
    Annotation* Find(int id);
    void Remove(int id);
    void Clear();

private:
    Shader m_MeshShader;
    Shader m_FlatShader;
    DepthLayers m_Layers;

    GLuint m_LineVao;
    GLuint m_LineVbo;

    std::vector<std::unique_ptr<Annotation>> m_Annotations;
    int m_NextId;
};
