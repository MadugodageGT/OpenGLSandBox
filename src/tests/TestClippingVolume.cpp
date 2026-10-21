#include "TestClippingVolume.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "Renderer.h"   // GLCall
#include "imgui/imgui.h"

namespace test {

    TestClippingVolume::TestClippingVolume()
        : m_TraceMode(true),
          m_PickRequested(false),
          m_PickMouse(0.0f),
          m_Request(Request::None),
          m_SelectedAnnotation(-1),
          m_ShowVolumes(false),
          m_ShowPolyline(true),
          m_ModelColor(0.80f, 0.78f, 0.72f)
    {
        try
        {
            m_Mesh.reset(new SurfaceMesh("res/models/suzanne.glb"));
            m_Camera.Frame(m_Mesh->Center(), m_Mesh->Radius());
            m_Status = "Right-drag to orbit, left-click to place outline points, Enter to create.";
        }
        catch (const std::exception& e)
        {
            m_LoadError = e.what();
        }
    }

    TestClippingVolume::~TestClippingVolume()
    {
    }

    void TestClippingVolume::HandleMouse()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
            return;

        GLFWwindow* window = glfwGetCurrentContext();
        const bool shift = window &&
            (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

        const bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        const bool mmb = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

        if (rmb || mmb)
        {
            GLint vp[4];
            glGetIntegerv(GL_VIEWPORT, vp);
            if (mmb || shift)
                m_Camera.Pan(io.MouseDelta.x, io.MouseDelta.y, static_cast<float>(vp[3]));
            else
                m_Camera.Orbit(-io.MouseDelta.x * 0.01f, io.MouseDelta.y * 0.01f);
        }

        if (io.MouseWheel != 0.0f)
            m_Camera.Zoom(io.MouseWheel);

        if (m_TraceMode && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_PickRequested = true;
            m_PickMouse = glm::vec2(io.MousePos.x, io.MousePos.y);
        }
    }

    void TestClippingVolume::OnUpdate(float)
    {
        if (m_Mesh)
            HandleMouse();
    }

    void TestClippingVolume::OnEvent(int key, int action)
    {
        if (action != GLFW_PRESS)
            return;

        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
            m_Request = Request::CreateNew;
        else if (key == GLFW_KEY_BACKSPACE && !m_Polyline.empty())
            m_Polyline.pop_back();
        else if (key == GLFW_KEY_ESCAPE)
            m_Polyline.clear();
    }

    void TestClippingVolume::ProcessCreateRequest(const glm::mat4& viewProj, int viewportW, int viewportH)
    {
        const Request request = m_Request;
        m_Request = Request::None;
        if (request == Request::None)
            return;

        int target = -1;
        if (request == Request::CreateInSelected)
        {
            if (!m_Annotations.Find(m_SelectedAnnotation))
            {
                m_Status = "Select an annotation first.";
                return;
            }
            target = m_SelectedAnnotation;
        }

        const AnnotationSystem::CreateResult result =
            m_Annotations.CreateFromPolyline(*m_Mesh, m_Polyline, viewProj, viewportW, viewportH, target, m_Params);

        m_Status = result.message;
        if (result.ok)
        {
            m_SelectedAnnotation = result.annotationId;
            m_Polyline.clear();
        }
    }

    void TestClippingVolume::OnRender()
    {
        if (!m_Mesh)
            return;

        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        const int W = vp[2];
        const int H = vp[3];
        if (W <= 0 || H <= 0)
            return;

        const glm::mat4 view = m_Camera.View();
        const glm::mat4 proj = m_Camera.Projection(static_cast<float>(W) / static_cast<float>(H));
        const glm::mat4 viewProj = proj * view;
        const glm::vec3 camPos = m_Camera.Position();

        // Annotation creation uses the matrix of the view the user is looking at right now.
        ProcessCreateRequest(viewProj, W, H);

        // The application already cleared colour + depth for this frame.
        GLCall(glEnable(GL_DEPTH_TEST));
        GLCall(glDepthFunc(GL_LESS));
        m_Annotations.DrawMesh(*m_Mesh, viewProj, camPos, m_ModelColor, 1.0f);

        // Picking reads the depth of the model that was just drawn.
        if (m_PickRequested)
        {
            m_PickRequested = false;
            const ImGuiIO& io = ImGui::GetIO();
            const int px = static_cast<int>(m_PickMouse.x * io.DisplayFramebufferScale.x);
            const int py = H - 1 - static_cast<int>(m_PickMouse.y * io.DisplayFramebufferScale.y);

            glm::vec3 hit;
            if (m_Annotations.PickSurfacePoint(px, py, viewProj, W, H, hit))
                m_Polyline.push_back(hit);
            else
                m_Status = "That click did not hit the model.";
        }

        // The annotations: two-pass stencil rendering, independent of the mesh representation.
        m_Annotations.RenderOverlays(*m_Mesh, viewProj, camPos, m_Overlay);

        if (m_ShowVolumes)
            m_Annotations.RenderVolumeWireframes(viewProj);
        if (m_ShowPolyline)
            m_Annotations.RenderPolyline(m_Polyline, viewProj);

        // The other tests expect depth testing to be off (see Application.cpp).
        GLCall(glDisable(GL_DEPTH_TEST));
    }

    void TestClippingVolume::OnImGuiRender()
    {
        if (!m_Mesh)
        {
            ImGui::TextWrapped("Could not load res/models/suzanne.glb: %s", m_LoadError.c_str());
            return;
        }

        ImGui::Text("Model: %zu triangles", m_Mesh->TriangleCount());
        ImGui::TextWrapped("RMB orbit | Shift+RMB or MMB pan | wheel zoom | LMB add point");
        ImGui::Separator();

        // ---- outline ------------------------------------------------------------------------------------
        ImGui::Checkbox("Trace mode (LMB adds points)", &m_TraceMode);
        ImGui::Text("Outline points: %zu", m_Polyline.size());
        if (ImGui::Button("Undo point") && !m_Polyline.empty())
            m_Polyline.pop_back();
        ImGui::SameLine();
        if (ImGui::Button("Clear outline"))
            m_Polyline.clear();

        if (ImGui::Button("Create annotation (Enter)"))
            m_Request = Request::CreateNew;
        if (m_Annotations.Find(m_SelectedAnnotation))
        {
            if (ImGui::Button("Add as extra volume to selected"))
                m_Request = Request::CreateInSelected;
        }
        if (!m_Status.empty())
            ImGui::TextWrapped("%s", m_Status.c_str());

        ImGui::Separator();

        // ---- construction parameters -------------------------------------------------------------------
        if (ImGui::CollapsingHeader("Volume construction"))
        {
            ImGui::SliderFloat("Depth margin", &m_Params.depthMargin, 1e-6f, 1e-3f, "%.6f",
                               ImGuiSliderFlags_Logarithmic);
            ImGui::Checkbox("Refine thin features", &m_Params.refineThinFeatures);
            ImGui::SliderInt("Max refine rounds", &m_Params.maxRefineRounds, 0, 12);
            ImGui::SliderFloat("Min triangle area (px^2)", &m_Params.minTriangleAreaPx, 2.0f, 200.0f, "%.0f");
        }

        // ---- display ------------------------------------------------------------------------------------
        if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Z-fail stencil (robust)", &m_Overlay.zFail);
            ImGui::SliderFloat("Tint opacity", &m_Overlay.alpha, 0.05f, 1.0f);
            ImGui::Checkbox("Show clipping volumes (wireframe)", &m_ShowVolumes);
            ImGui::Checkbox("Show outline", &m_ShowPolyline);
            ImGui::ColorEdit3("Model colour", &m_ModelColor.x);
        }

        // ---- annotation list ----------------------------------------------------------------------------
        ImGui::Separator();
        ImGui::Text("Annotations (%zu)", m_Annotations.Annotations().size());

        int removeId = -1;
        std::vector<std::unique_ptr<Annotation>>& list = m_Annotations.Annotations();
        for (size_t i = 0; i < list.size(); ++i)
        {
            Annotation& a = *list[i];
            ImGui::PushID(a.id);

            ImGui::Checkbox("##visible", &a.visible);
            ImGui::SameLine();
            ImGui::ColorEdit3("##color", &a.color.x, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            if (ImGui::Selectable(a.name.c_str(), m_SelectedAnnotation == a.id))
                m_SelectedAnnotation = a.id;
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeId = a.id;

            if (m_SelectedAnnotation == a.id)
            {
                ImGui::Text("  %zu volume(s), %zu triangles", a.volumes.size(), a.TriangleCount());
                ImGui::Text("  perimeter %.3f, area ~%.3f", a.perimeter, a.area);
            }
            ImGui::PopID();
        }
        if (removeId >= 0)
        {
            if (removeId == m_SelectedAnnotation)
                m_SelectedAnnotation = -1;
            m_Annotations.Remove(removeId);
        }
    }

}
