#pragma once

#include "Test.h"

#include "VertexArray.h"
#include "IndexBuffer.h"
#include "VertexBufferLayout.h"
#include "Renderer.h"
#include "Shader.h"


#include "glm/glm.hpp"

namespace test{
	class TestQuadColor : public Test
	{
	public:
		TestQuadColor();
		~TestQuadColor();

		void OnUpdate(float deltaTime) override;
		void OnRender() override;
		void OnImGuiRender() override;

	private:

		static unsigned int s_Indices[6];
		static float s_Vertices[8];

		VertexArray m_VA;
		VertexBuffer m_VB;
		IndexBuffer m_IB;
		Shader m_Shader;

		glm::mat4 m_Proj;
		glm::mat4 m_View;
		glm::mat4 m_Model;

		Renderer m_Renderer;

		float m_QuadColor[4];

	};
}