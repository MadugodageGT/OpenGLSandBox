#include "TestTexture2D.h"

#include "imgui/ImGui.h"

namespace test {

	unsigned int TestTexture2D::s_Indices[6] = {
												0, 1, 2,
												2, 3, 0 };

	float TestTexture2D::s_Vertices[16] = {
										-50.0, -50.0,     0.0f, 0.0f,
										50.0, -50.0,      1.0f, 0.0f,
										50.0, 50.0,       1.0f, 1.0f,
										-50.0, 50.0,      0.0f, 1.0f };

	TestTexture2D::TestTexture2D()
		:
		m_VB(s_Vertices, 16 * sizeof(float)),
		m_IB(s_Indices, 6),
		m_Shader("res/shaders/BasicTexture.shader"),
		m_ModelPosition(glm::vec3(480.0f, 270.0f, 0.0f)),
		m_Texture("res/textures/T_PinkRainLily_Albedo_4k.png")
	{

		m_Proj = glm::ortho(0.0f, 960.0f, 0.0f, 540.0f, -1.0f, 1.0f);
		m_View = glm::mat4(1.0f);
		m_Model = glm::mat4(1.0f);


		m_VA.Bind();

		VertexBufferLayout layout;
		layout.Push<float>(2);
		layout.Push<float>(2);

		m_VA.AddBuffer(m_VB, layout);

		m_Shader.Bind();
		m_Texture.Bind();
		m_Shader.setUniform1i("u_Texture",0);


		m_Shader.Unbind();

	}

	TestTexture2D::~TestTexture2D()
	{

	}

	void TestTexture2D::OnUpdate(float deltaTime)
	{
		m_Renderer.Clear();

		m_Shader.Bind();

		m_Model = glm::translate(glm::mat4(1.0f), m_ModelPosition);
		glm::mat4 mvp = m_Proj * m_View * m_Model;

		m_Shader.setUniformMat4f("u_MVP", mvp);

		m_Shader.Unbind();
	}

	void TestTexture2D::OnRender()
	{



		m_Renderer.Draw(m_VA, m_IB, m_Shader);

	}

	void TestTexture2D::OnImGuiRender()
	{
		ImGui::SliderFloat("X", &m_ModelPosition.x, 0.0f, 960.0f);
		ImGui::SliderFloat("Y", &m_ModelPosition.y, 0.0f, 540.0f);
	}

}