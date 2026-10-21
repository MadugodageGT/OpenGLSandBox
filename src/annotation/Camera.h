#pragma once

#include "glm/glm.hpp"

// OrbitCamera - a small turntable camera: it orbits a target point at a given distance.
// It has no OpenGL or input-library dependency; the owner feeds it mouse deltas.
//
//   Orbit(dYaw, dPitch)   rotate around the target (radians)
//   Pan(dx, dy, viewH)    slide the target in the image plane (pixel deltas)
//   Zoom(steps)           dolly towards (+) / away from (-) the target
//
// Near/far planes follow the camera distance and the scene radius, which keeps the depth buffer
// precise (the annotation passes compare depths, so precision matters).

class OrbitCamera
{
public:
    OrbitCamera();

    // Look at a bounding sphere from a comfortable distance.
    void Frame(const glm::vec3& center, float sceneRadius);

    void Orbit(float dYaw, float dPitch);
    void Pan(float dxPixels, float dyPixels, float viewportHeightPixels);
    void Zoom(float wheelSteps);

    void SetAngles(float yaw, float pitch) { m_Yaw = yaw; m_Pitch = pitch; ClampPitch(); }
    float Yaw() const { return m_Yaw; }
    float Pitch() const { return m_Pitch; }

    glm::vec3 Position() const;
    glm::vec3 Forward() const;
    glm::mat4 View() const;
    glm::mat4 Projection(float aspect) const;

    float NearPlane() const;
    float FarPlane() const;

private:
    void ClampPitch();

    glm::vec3 m_Target;
    float m_Distance;
    float m_Yaw;     // around +Y
    float m_Pitch;   // above the horizon
    float m_SceneRadius;
    float m_FovY;    // radians
};
