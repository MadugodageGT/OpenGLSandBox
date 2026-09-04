#pragma once

#include "Test.h"

#include "VertexArray.h"
#include "IndexBuffer.h"
#include "VertexBufferLayout.h"
#include "Renderer.h"
#include "Shader.h"
#include "Texture.h"

#include "glm/glm.hpp"

namespace test{

	class TestBatchRendering : public Test
	{
	public:
		TestBatchRendering();
		~TestBatchRendering();

		void OnUpdate(float deltaTime) override;
		void OnRender() override;
		void OnImGuiRender() override;

		void OnEvent(int key, int action) override;
	private:



		Renderer m_Renderer;

		Shader m_Shader;

		VertexArray m_VA;
		VertexBuffer m_VB;
		IndexBuffer m_IB;

		glm::mat4 m_Proj;
		glm::mat4 m_View;
		glm::mat4 m_Model;

		Texture m_Texture_1;
		Texture m_Texture_2;

		float m_QuadPosition[2] = { -0.9, -0.5 };
	};


}