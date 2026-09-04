#pragma once

#include "Test.h"

#include "VertexArray.h"
#include "IndexBuffer.h"
#include "VertexBufferLayout.h"
#include "Renderer.h"
#include "Shader.h"
#include "Texture.h"

#include "glm/glm.hpp"

namespace test {
	class TestTexture2D : public Test
	{
	public:
		TestTexture2D();
		~TestTexture2D();

		void OnUpdate(float deltaTime) override;
		void OnRender() override;
		void OnImGuiRender() override;

	private:

		static unsigned int s_Indices[6];
		static float s_Vertices[16];

		VertexArray m_VA;
		VertexBuffer m_VB;
		IndexBuffer m_IB;
		Shader m_Shader;

		glm::mat4 m_Proj;
		glm::mat4 m_View;
		glm::mat4 m_Model;

		glm::vec3 m_ModelPosition;

		Renderer m_Renderer;
		Texture m_Texture;

	};
}