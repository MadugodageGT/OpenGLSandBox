#pragma once

#include "Test.h"

#include <cstring>
#include <stdexcept>
#include <vector>

#include "VertexArray.h"
#include "IndexBuffer.h"
#include "VertexBufferLayout.h"
#include "Renderer.h"
#include "Shader.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "tinygltf/tiny_gltf_v3.h"


namespace test {
	class TestLoadingMesh : public Test
	{
	public:
		TestLoadingMesh();
		~TestLoadingMesh();

		void OnUpdate(float deltaTime) override;
		void OnRender() override;
		void OnImGuiRender() override;

    private:

        GLuint m_MeshVAO = 0;
        GLuint m_PositionVBO = 0;
        GLuint m_IndexEBO = 0;

        GLsizei m_IndexCount = 0;
        GLenum m_IndexType = GL_UNSIGNED_INT;

        Shader m_Shader;
		Shader m_OutlineShader;

        glm::mat4 m_Proj;
        glm::mat4 m_View;
        glm::mat4 m_Model;

        glm::vec3 m_ModelPosition;
        float m_ModelRotation;

        Renderer m_Renderer;

	};
}