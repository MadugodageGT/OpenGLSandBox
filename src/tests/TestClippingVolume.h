#pragma once

#include "Test.h"

#include <memory>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "annotation/AnnotationSystem.h"
#include "annotation/Camera.h"
#include "annotation/ClippingVolume.h"
#include "annotation/SurfaceMesh.h"

// TestClippingVolume - interactive demo of the ClippingVolumes annotation method
// (Ponchio et al., "Effective Annotations over 3D Models", CGF 2019).
//
// Controls
//   Right mouse drag            orbit            Shift + right drag / middle drag   pan
//   Mouse wheel                 zoom             Left click                          add an outline point on the surface
//   Enter                       create annotation from the outline
//   Backspace                   remove the last point      Escape   clear the outline
//
// Workflow: orbit to a good view, click points around the region (the outline is closed automatically),
// press "Create annotation". The region is stored as a small closed mesh (the clipping volume); it keeps
// highlighting exactly that region of the surface from every camera angle. A region that wraps around
// the model is built from several views with "Add as extra volume to selected".

namespace test {

    class TestClippingVolume : public Test
    {
    public:
        TestClippingVolume();
        ~TestClippingVolume();

        void OnUpdate(float deltaTime) override;
        void OnRender() override;
        void OnImGuiRender() override;
        void OnEvent(int key, int action) override;

    private:
        enum class Request { None, CreateNew, CreateInSelected };

        void HandleMouse();
        void ProcessCreateRequest(const glm::mat4& viewProj, int viewportW, int viewportH);

        std::unique_ptr<SurfaceMesh> m_Mesh;
        std::string m_LoadError;

        AnnotationSystem m_Annotations;
        OrbitCamera m_Camera;

        // outline being traced (world-space points picked on the surface)
        std::vector<glm::vec3> m_Polyline;
        bool m_TraceMode;
        bool m_PickRequested;
        glm::vec2 m_PickMouse;      // window coordinates of the click (ImGui space, y down)
        Request m_Request;
        int m_SelectedAnnotation;

        // settings
        clipvol::BuildParams m_Params;
        OverlaySettings m_Overlay;
        bool m_ShowVolumes;
        bool m_ShowPolyline;
        glm::vec3 m_ModelColor;

        std::string m_Status;
    };

}
