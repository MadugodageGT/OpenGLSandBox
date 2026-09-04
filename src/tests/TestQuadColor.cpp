#include "TestQuadColor.h"

#include "imgui/ImGui.h"

namespace test {

	unsigned int TestQuadColor::s_Indices[6] = {
												0, 1, 2,
												2, 3, 0 };

	float TestQuadColor::s_Vertices[8] = { 
											-50.0, -50.0,
											50.0, -50.0,
											50.0, 50.0, 
											-50.0, 50.0 };

	TestQuadColor::TestQuadColor()
		:
		m_VB(s_Vertices, 8 * sizeof(float)),
		m_IB(s_Indices, 6),
		m_Shader("res/shaders/Basic.shader"),
		m_QuadColor{ 0.2f, 0.4f, 0.8f, 1.0f }
	{

		m_Proj = glm::ortho(0.0f, 960.0f, 0.0f, 540.0f, -1.0f, 1.0f);
		m_View = glm::mat4(1.0f);
		m_Model = glm::mat4(1.0f);
			   

		m_VA.Bind();

		VertexBufferLayout layout;
		layout.Push<float>(2);

		m_VA.AddBuffer(m_VB, layout);

	}

	TestQuadColor::~TestQuadColor()
	{

	}

	void TestQuadColor::OnUpdate(float deltaTime)
	{
		m_Renderer.Clear();

		m_Shader.Bind();

		m_Model = glm::translate(glm::mat4(1.0f), glm::vec3(480, 270, 0));
		glm::mat4 mvp = m_Proj * m_View * m_Model;

		m_Shader.setUniformMat4f("u_MVP", mvp);
		m_Shader.setUniform4f("u_Color", m_QuadColor[0], m_QuadColor[1], m_QuadColor[2], m_QuadColor[3]);

		m_Shader.Unbind();
	}

	void TestQuadColor::OnRender()
	{
		m_Renderer.Draw( m_VA, m_IB, m_Shader);
	}

	void TestQuadColor::OnImGuiRender()
	{
		ImGui::ColorEdit4("Quad Color", m_QuadColor);
	}

}