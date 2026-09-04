#include "TestIsoMetric.h"

#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "imgui/ImGui.h" 

#include <array>

namespace test {

	struct Vertex {
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoords;
		float TexID;
	};

	static Vertex* CreateQuad(Vertex* target, float x, float y, unsigned int slot )
	{
		float width = 32.0f;
		float height = 32.0f;

		target->Position = { x, y, 0.0f }; // Bottom-Left
		target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		target->TexCoords = { 0.0f + (slot * 0.25), 0.0f};
		target->TexID = 0.0f;
		target++;

		target->Position = { x + width, y, 0.0f }; // Bottom-Right
		target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		target->TexCoords = { 0.25f+(slot * 0.25), 0.0f };
		target->TexID = 0.0f;
		target++;


		target->Position = { x + width, y + height, 0.0f }; // Top-Right
		target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		target->TexCoords = { 0.25f + (slot * 0.25), 1.0f };
		target->TexID = 0.0f;
		target++;

		target->Position = { x, y + height, 0.0f }; // Top-Left
		target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		target->TexCoords = { 0.0f + (slot * 0.25), 1.0f };
		target->TexID = 0.0f;
		target++;

		return target;
	}




	const size_t MaxQuadCount = 1000;
	const size_t MaxVertexCount = MaxQuadCount * 4;
	const size_t MaxIndexCount = MaxQuadCount * 6;

	TestIsoMetric::TestIsoMetric()
		:m_Shader("res/shaders/IsoMetric.shader"),
		m_VB(nullptr, sizeof(Vertex) * MaxVertexCount),
		m_IB(nullptr, MaxIndexCount),
		m_Texture_1("res/textures/T_Ground_Sprite_32.png"),
		m_Texture_2("res/textures/T_PinkRainLily_Albedo_4k.png")

	{
		
		unsigned int indices[MaxIndexCount];
		unsigned int offset = 0;
		for (size_t i = 0; i < MaxIndexCount; i += 6)
		{
			indices[i + 0] = 0 + offset;
			indices[i + 1] = 1 + offset;
			indices[i + 2] = 2 + offset;

			indices[i + 3] = 2 + offset;
			indices[i + 4] = 3 + offset;
			indices[i + 5] = 0 + offset;

			offset += 4;
		}

		m_VA.Bind();

		VertexBufferLayout layout;
		layout.Push<float>(3);
		layout.Push<float>(4);
		layout.Push<float>(2);
		layout.Push<float>(1);

		m_VA.AddBuffer(m_VB, layout);

		m_IB.Update(indices,MaxIndexCount);

		m_Proj = glm::ortho(0.0f, 1080.0f, 0.0f, 720.0f, -1.0f, 1.0f);
		m_View = glm::mat4(1.0f);
		m_Model = glm::mat4(1.0f);

		m_Shader.Bind();

		m_Texture_1.Bind(0);
		m_Texture_2.Bind(1);

		int samplers[] = { 0, 1 };
		m_Shader.setUniform1iv("u_Texture", 2, samplers);

		m_Shader.Unbind();

	}

	TestIsoMetric::~TestIsoMetric()
	{

	}

	void TestIsoMetric::OnUpdate(float deltaTime)
	{
		m_Renderer.Clear();

		float x = 524.0f;
		float y = 520.0f;

		std::array <Vertex, MaxVertexCount> vertices;
		Vertex* buffer = vertices.data();
		for (int i = 0; i < 20; i++)
		{
			for (int j = 0; j < 20; j++)
			{
				buffer = CreateQuad(buffer, x+(16.0f*j)-(16.0f*i), y-(8.0f*j)-(8.0f*i), 0);
			}
		}

		float alpha = x + (0.5 * m_QuadPosition[0]) - (0.5 * m_QuadPosition[1]);
		float beta = y - (0.25 * m_QuadPosition[0]) - (0.25 * m_QuadPosition[1]);

		buffer = CreateQuad(buffer, alpha , beta, 2);

		m_VA.Bind();
		GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data()));


		m_Shader.Bind();

		glm::mat4 mvp = m_Proj * m_View * m_Model;

		m_Shader.setUniformMat4f("u_MVP", mvp);

		m_Shader.Unbind();

	}

	void TestIsoMetric::OnRender()
	{
		m_Renderer.Draw(m_VA, m_IB, m_Shader);
	}

	void TestIsoMetric::OnImGuiRender()
	{
		ImGui::Begin("Controls");
		ImGui::DragFloat2("Quad Position", m_QuadPosition, 0.1f);
		ImGui::End();

	}

	void TestIsoMetric::OnEvent(int key, int action)
	{
		if (key == GLFW_KEY_W && action == GLFW_PRESS)
		{
			//std::cout << "W pressed \n";
			m_QuadPosition[0] += 10;
			
		}

		if (key == GLFW_KEY_S && action == GLFW_PRESS)
		{
			//std::cout << "W pressed \n";
			m_QuadPosition[0] -= 10;

		}

		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			std::cout << "Escape pressed \n";
		}
	}

}