#include "AnnotationSystem.h"

#include <algorithm>
#include <cstdio>

#include "Renderer.h"   // GLCall
#include "annotation/GLStateGuard.h"
#include "annotation/SurfaceMesh.h"

namespace {

const glm::vec4 kPalette[] = {
    glm::vec4(0.95f, 0.25f, 0.20f, 1.0f),
    glm::vec4(0.20f, 0.55f, 0.95f, 1.0f),
    glm::vec4(0.20f, 0.80f, 0.35f, 1.0f),
    glm::vec4(0.95f, 0.75f, 0.10f, 1.0f),
    glm::vec4(0.75f, 0.30f, 0.90f, 1.0f),
    glm::vec4(0.10f, 0.80f, 0.80f, 1.0f),
};
const int kPaletteSize = static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));

} // namespace

// ---------------------------------------------------------------------------------------------
// VolumeMesh
// ---------------------------------------------------------------------------------------------

VolumeMesh::VolumeMesh(const clipvol::BuildResult& built)
    : m_Vao(0), m_Vbo(0), m_Ebo(0),
      m_IndexCount(static_cast<GLsizei>(built.indices.size())),
      m_RefineRounds(built.refineRounds)
{
    glGenVertexArrays(1, &m_Vao);
    glBindVertexArray(m_Vao);

    glGenBuffers(1, &m_Vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_Vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(built.positions.size() * sizeof(glm::vec3)),
                 built.positions.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<const void*>(0));

    glGenBuffers(1, &m_Ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(built.indices.size() * sizeof(uint32_t)),
                 built.indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

VolumeMesh::~VolumeMesh()
{
    if (m_Ebo) glDeleteBuffers(1, &m_Ebo);
    if (m_Vbo) glDeleteBuffers(1, &m_Vbo);
    if (m_Vao) glDeleteVertexArrays(1, &m_Vao);
}

void VolumeMesh::Draw() const
{
    glBindVertexArray(m_Vao);
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------------------------
// AnnotationSystem
// ---------------------------------------------------------------------------------------------

AnnotationSystem::AnnotationSystem()
    : m_MeshShader("res/shaders/ClipMesh.shader"),
      m_FlatShader("res/shaders/ClipFlat.shader"),
      m_LineVao(0), m_LineVbo(0), m_NextId(1)
{
    glGenVertexArrays(1, &m_LineVao);
    glBindVertexArray(m_LineVao);
    glGenBuffers(1, &m_LineVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<const void*>(0));
    glBindVertexArray(0);
}

AnnotationSystem::~AnnotationSystem()
{
    m_Annotations.clear();   // releases the volume buffers while the context is still alive
    if (m_LineVbo) glDeleteBuffers(1, &m_LineVbo);
    if (m_LineVao) glDeleteVertexArrays(1, &m_LineVao);
}

Annotation* AnnotationSystem::Find(int id)
{
    for (size_t i = 0; i < m_Annotations.size(); ++i)
    {
        if (m_Annotations[i]->id == id)
            return m_Annotations[i].get();
    }
    return nullptr;
}

void AnnotationSystem::Remove(int id)
{
    for (size_t i = 0; i < m_Annotations.size(); ++i)
    {
        if (m_Annotations[i]->id == id)
        {
            m_Annotations.erase(m_Annotations.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

void AnnotationSystem::Clear()
{
    m_Annotations.clear();
}

bool AnnotationSystem::PickSurfacePoint(int px, int py, const glm::mat4& viewProj,
                                        int viewportW, int viewportH, glm::vec3& outWorld) const
{
    if (px < 0 || py < 0 || px >= viewportW || py >= viewportH)
        return false;

    float depth = 1.0f;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    if (!clipvol::IsSurface(depth))
        return false;

    const glm::vec4 ndc(2.0f * (static_cast<float>(px) + 0.5f) / static_cast<float>(viewportW) - 1.0f,
                        2.0f * (static_cast<float>(py) + 0.5f) / static_cast<float>(viewportH) - 1.0f,
                        2.0f * depth - 1.0f,
                        1.0f);
    const glm::vec4 w = glm::inverse(viewProj) * ndc;
    outWorld = glm::vec3(w) / w.w;
    return true;
}

AnnotationSystem::CreateResult AnnotationSystem::CreateFromPolyline(
    const SurfaceMesh& mesh,
    const std::vector<glm::vec3>& polylineWorld,
    const glm::mat4& viewProj, int viewportW, int viewportH,
    int targetAnnotation,
    const clipvol::BuildParams& params)
{
    CreateResult out;

    if (polylineWorld.size() < 3)
    {
        out.message = "Add at least 3 points before creating an annotation.";
        return out;
    }

    // 1. Project the picked surface points into view space (window pixels). The clipping direction is the
    //    current viewing direction, exactly as in the paper.
    clipvol::BuildInput input;
    input.viewProj = viewProj;
    for (size_t i = 0; i < polylineWorld.size(); ++i)
    {
        const glm::vec4 clip = viewProj * glm::vec4(polylineWorld[i], 1.0f);
        if (clip.w <= 1e-6f)
        {
            out.message = "A picked point is behind the camera.";
            return out;
        }
        const glm::vec2 ndc = glm::vec2(clip) / clip.w;
        input.polygonPx.push_back((ndc * 0.5f + 0.5f) * glm::vec2(static_cast<float>(viewportW),
                                                                  static_cast<float>(viewportH)));
    }

    // 2. Rasterise the surface in view space (nearest layer + second layer).
    clipvol::DepthMap layer1, layer2;
    if (!m_Layers.Capture(mesh, viewProj, viewportW, viewportH, layer1, layer2))
    {
        out.message = "Depth capture failed (framebuffer incomplete).";
        return out;
    }
    input.layer1 = &layer1;
    input.layer2 = &layer2;

    // 3. Build the clipping prism (triangulate, offsets, refine, extrude).
    const clipvol::BuildResult built = clipvol::BuildClippingVolume(input, params);
    if (!built.ok)
    {
        out.message = "Could not create the annotation: " + built.error + ".";
        return out;
    }

    // 4. Numeric attributes computed at creation time (paper Sec. 6.2).
    float perimeter = 0.0f;
    for (size_t i = 0; i < polylineWorld.size(); ++i)
        perimeter += glm::distance(polylineWorld[i], polylineWorld[(i + 1) % polylineWorld.size()]);
    const float area = clipvol::ApproximateEnclosedArea(built, viewProj, viewportW, viewportH,
                                                        mesh.Positions(), mesh.Indices());

    // 5. Store: the annotation is just a small closed mesh (a few hundred triangles at most).
    Annotation* target = targetAnnotation >= 0 ? Find(targetAnnotation) : nullptr;
    const bool isNew = (target == nullptr);
    if (isNew)
    {
        std::unique_ptr<Annotation> a(new Annotation());
        a->id = m_NextId++;
        char name[48];
        std::snprintf(name, sizeof(name), "Annotation %d", a->id);
        a->name = name;
        a->color = kPalette[(a->id - 1) % kPaletteSize];
        target = a.get();
        m_Annotations.push_back(std::move(a));
    }

    target->volumes.push_back(std::unique_ptr<VolumeMesh>(new VolumeMesh(built)));
    target->perimeter += perimeter;
    target->area += area;

    char msg[200];
    std::snprintf(msg, sizeof(msg), "%s '%s': %zu triangles, %d refinement round(s), area ~%.3f.",
                  isNew ? "Created" : "Added a volume to", target->name.c_str(),
                  built.indices.size() / 3, built.refineRounds, area);
    out.ok = true;
    out.annotationId = target->id;
    out.message = msg;
    return out;
}

void AnnotationSystem::DrawMesh(const SurfaceMesh& mesh, const glm::mat4& viewProj, const glm::vec3& camPos,
                                const glm::vec3& color, float alpha)
{
    m_MeshShader.Bind();
    m_MeshShader.setUniformMat4f("u_MVP", viewProj);
    m_MeshShader.setUniform4f("u_CamPos", camPos.x, camPos.y, camPos.z, 1.0f);
    m_MeshShader.setUniform4f("u_Color", color.r, color.g, color.b, 1.0f);
    m_MeshShader.setUniform1f("u_Alpha", alpha);
    mesh.Draw();
}

void AnnotationSystem::RenderOverlays(const SurfaceMesh& mesh, const glm::mat4& viewProj,
                                      const glm::vec3& camPos, const OverlaySettings& settings)
{
    GLStateGuard guard;

    GLCall(glEnable(GL_STENCIL_TEST));
    GLCall(glEnable(GL_DEPTH_TEST));
    GLCall(glEnable(GL_DEPTH_CLAMP));   // volumes poking past the far plane must not be clipped (z-fail)
    GLCall(glDisable(GL_CULL_FACE));
    GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    for (size_t a = 0; a < m_Annotations.size(); ++a)
    {
        const Annotation& ann = *m_Annotations[a];
        if (!ann.visible || ann.volumes.empty())
            continue;

        // Each annotation (colour) gets its own two-pass cycle, so overlapping annotations stay independent.
        GLCall(glStencilMask(0xFF));
        GLCall(glClear(GL_STENCIL_BUFFER_BIT));

        // ---- Pass 1: fill the stencil buffer with the clipping volumes (shadow-volume style). --------
        // No colour, no depth writes; depth *test* against the model already in the depth buffer.
        GLCall(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
        GLCall(glDepthMask(GL_FALSE));
        GLCall(glDepthFunc(GL_LESS));
        GLCall(glStencilFunc(GL_ALWAYS, 0, 0xFF));

        if (settings.zFail)
        {
            // Depth-fail: a pixel ends up non-zero when the model surface lies between the volume's
            // front and back faces. Works even if the camera is inside a volume (volumes are closed).
            GLCall(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP));
            GLCall(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP));
        }
        else
        {
            // Depth-pass: count the volume faces in front of the surface (cheaper, but breaks if the
            // camera enters a volume).
            GLCall(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP));
            GLCall(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP));
        }

        m_FlatShader.Bind();
        m_FlatShader.setUniformMat4f("u_MVP", viewProj);
        m_FlatShader.setUniform4f("u_Color", 0.0f, 0.0f, 0.0f, 0.0f);
        m_FlatShader.setUniform1f("u_PointSize", 1.0f);
        for (size_t v = 0; v < ann.volumes.size(); ++v)
            ann.volumes[v]->Draw();   // overlapping volumes of one annotation simply count higher: union

        // ---- Pass 2: tint the model wherever the stencil is non-zero. -----------------------------------
        GLCall(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        GLCall(glStencilFunc(GL_NOTEQUAL, 0, 0xFF));
        GLCall(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
        GLCall(glStencilMask(0x00));
        GLCall(glDepthFunc(GL_LEQUAL));   // same program, same matrix => identical depth to the first pass
        GLCall(glEnable(GL_BLEND));

        DrawMesh(mesh, viewProj, camPos, glm::vec3(ann.color), settings.alpha);
    }
}

void AnnotationSystem::RenderVolumeWireframes(const glm::mat4& viewProj)
{
    GLStateGuard guard;

    GLCall(glEnable(GL_DEPTH_TEST));
    GLCall(glDepthFunc(GL_LEQUAL));
    GLCall(glDepthMask(GL_FALSE));
    GLCall(glDisable(GL_CULL_FACE));
    GLCall(glEnable(GL_BLEND));
    GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GLCall(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));

    m_FlatShader.Bind();
    m_FlatShader.setUniformMat4f("u_MVP", viewProj);
    m_FlatShader.setUniform1f("u_PointSize", 1.0f);
    for (size_t a = 0; a < m_Annotations.size(); ++a)
    {
        const Annotation& ann = *m_Annotations[a];
        if (!ann.visible)
            continue;
        m_FlatShader.setUniform4f("u_Color", ann.color.r, ann.color.g, ann.color.b, 0.85f);
        for (size_t v = 0; v < ann.volumes.size(); ++v)
            ann.volumes[v]->Draw();
    }
}

void AnnotationSystem::RenderPolyline(const std::vector<glm::vec3>& points, const glm::mat4& viewProj)
{
    if (points.empty())
        return;

    GLStateGuard guard;

    GLCall(glDisable(GL_DEPTH_TEST));
    GLCall(glEnable(GL_BLEND));
    GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GLCall(glEnable(GL_PROGRAM_POINT_SIZE));

    GLCall(glBindVertexArray(m_LineVao));
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, m_LineVbo));
    GLCall(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(points.size() * sizeof(glm::vec3)),
                        points.data(), GL_DYNAMIC_DRAW));

    m_FlatShader.Bind();
    m_FlatShader.setUniformMat4f("u_MVP", viewProj);

    if (points.size() >= 2)
    {
        m_FlatShader.setUniform4f("u_Color", 1.0f, 0.95f, 0.2f, 1.0f);
        m_FlatShader.setUniform1f("u_PointSize", 1.0f);
        GLCall(glDrawArrays(points.size() >= 3 ? GL_LINE_LOOP : GL_LINE_STRIP, 0,
                            static_cast<GLsizei>(points.size())));
    }

    m_FlatShader.setUniform4f("u_Color", 1.0f, 1.0f, 1.0f, 1.0f);
    m_FlatShader.setUniform1f("u_PointSize", 9.0f);
    GLCall(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points.size())));

    m_FlatShader.setUniform4f("u_Color", 0.9f, 0.1f, 0.1f, 1.0f);
    m_FlatShader.setUniform1f("u_PointSize", 5.0f);
    GLCall(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points.size())));

    GLCall(glBindVertexArray(0));
}
