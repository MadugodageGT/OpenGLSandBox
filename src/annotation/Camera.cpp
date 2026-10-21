#include "Camera.h"

#include <algorithm>
#include <cmath>

#include "glm/gtc/matrix_transform.hpp"

OrbitCamera::OrbitCamera()
    : m_Target(0.0f),
      m_Distance(4.0f),
      m_Yaw(0.0f),
      m_Pitch(0.0f),
      m_SceneRadius(1.0f),
      m_FovY(glm::radians(45.0f))
{
}

void OrbitCamera::Frame(const glm::vec3& center, float sceneRadius)
{
    m_Target = center;
    m_SceneRadius = (std::max)(sceneRadius, 1e-4f);
    m_Distance = m_SceneRadius / std::sin(m_FovY * 0.5f) * 1.15f;
}

void OrbitCamera::ClampPitch()
{
    const float limit = glm::radians(89.0f);
    m_Pitch = (std::min)(limit, (std::max)(-limit, m_Pitch));
}

void OrbitCamera::Orbit(float dYaw, float dPitch)
{
    m_Yaw += dYaw;
    m_Pitch += dPitch;
    ClampPitch();
}

void OrbitCamera::Pan(float dxPixels, float dyPixels, float viewportHeightPixels)
{
    if (viewportHeightPixels <= 0.0f)
        return;

    const glm::vec3 forward = Forward();
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up = glm::cross(right, forward);

    const float unitsPerPixel = 2.0f * m_Distance * std::tan(m_FovY * 0.5f) / viewportHeightPixels;
    m_Target += (-right * dxPixels + up * dyPixels) * unitsPerPixel;
}

void OrbitCamera::Zoom(float wheelSteps)
{
    m_Distance *= std::pow(0.9f, wheelSteps);
    m_Distance = (std::max)(m_SceneRadius * 0.1f, (std::min)(m_SceneRadius * 40.0f, m_Distance));
}

glm::vec3 OrbitCamera::Position() const
{
    const float cp = std::cos(m_Pitch);
    return m_Target + m_Distance * glm::vec3(cp * std::sin(m_Yaw), std::sin(m_Pitch), cp * std::cos(m_Yaw));
}

glm::vec3 OrbitCamera::Forward() const
{
    return glm::normalize(m_Target - Position());
}

glm::mat4 OrbitCamera::View() const
{
    return glm::lookAt(Position(), m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
}

float OrbitCamera::NearPlane() const
{
    return (std::max)(m_SceneRadius * 0.02f, m_Distance - m_SceneRadius * 1.5f);
}

float OrbitCamera::FarPlane() const
{
    return m_Distance + m_SceneRadius * 1.5f;
}

glm::mat4 OrbitCamera::Projection(float aspect) const
{
    return glm::perspective(m_FovY, aspect, NearPlane(), FarPlane());
}
