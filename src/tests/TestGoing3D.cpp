#include "TestGoing3D.h"

#include "imgui/ImGui.h"

namespace test {

	unsigned int TestGoing3D::s_Indices[6] = {
												0, 1, 2,
												2, 3, 0 };

	float TestGoing3D::s_Vertices[16] = {
										-50.0, -50.0,     0.0f, 0.0f,
										50.0, -50.0,      1.0f, 0.0f,
										50.0, 50.0,       1.0f, 1.0f,
										-50.0, 50.0,      0.0f, 1.0f };

	TestGoing3D::TestGoing3D()
		:
		m_VB(s_Vertices, 16 * sizeof(float)),
		m_IB(s_Indices, 6),
		m_Shader("res/shaders/BasicTexture.shader"),
		m_ModelPosition(glm::vec3(480.0f,270.0f,0.0f)),
		m_ModelRotation(0.0f),
		m_Texture("res/textures/T_PinkRainLily_Albedo_4k.png")
	{

		m_Proj = glm::perspective(
		glm::radians(45.0f),
			960.0f/540.0f,
			0.1f,
			1000.0f
		);
		m_View = glm::lookAt(
			glm::vec3(480.0f, 270.0f, -480.0f),
			glm::vec3(480.0f,270.0f,0.0f),
			glm::vec3(0.0f,1.0f,0.0f)
		);
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

	TestGoing3D::~TestGoing3D()
	{

	}

	void TestGoing3D::OnUpdate(float deltaTime)
	{
		m_Renderer.Clear();
	}

	void TestGoing3D::OnRender()
	{
		GLCall(glEnable(GL_DEPTH_TEST));

		m_Model = glm::translate(glm::mat4(1.0f), m_ModelPosition);
		m_Model = glm::rotate(m_Model,glm::radians(m_ModelRotation), glm::vec3(0.0f, 1.0f, 0.0f));

		glm::mat4 mvp = m_Proj * m_View * m_Model;

		m_Shader.Bind();
		m_Shader.setUniformMat4f("u_MVP", mvp);

		m_Renderer.Draw(m_VA, m_IB, m_Shader);

		GLCall(glDisable(GL_DEPTH_TEST));
	}

	void TestGoing3D::OnImGuiRender()
	{
		{
			ImGui::SliderFloat("X", &m_ModelPosition.x, 0.0f, 960.0f);
			ImGui::SliderFloat("Y", &m_ModelPosition.y, 0.0f, 540.0f);
			ImGui::SliderFloat("Z", &m_ModelPosition.z, -1000.0f, 1000.0f);
		}
		{
			ImGui::SliderFloat("Rotation", &m_ModelRotation, 0.0f, 360.0f);

		}
	
	}

}