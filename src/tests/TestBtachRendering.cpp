#include "TestBatchRendering.h"

#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "imgui/ImGui.h" 

#include <array>

namespace test {

	struct vec2 {
		float x, y;
	};

	struct vec3 {
		float x, y, z;
	};

	struct vec4 {
		float x, y, z, w;
	};


	struct Vertex {
		vec3 Position;
		vec4 Color;
		vec2 TexCoords;
		float TexID;
	};

	static unsigned int s_Indices[] =
	{
		0,1,2,//left
		2,3,0,

		4,5,6,//right
		6,7,4
	};




	static std::array<Vertex, 4> CreateQuad(float x, float y)
	{
		float width = 0.5f;
		float height = 0.5f;

		Vertex v0;
		v0.Position = { x, y, 0.0f }; // Bottom-Left
		v0.Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		v0.TexCoords = { 0.0f, 0.0f };
		v0.TexID = 0.0f;

		Vertex v1;
		v1.Position = { x + width, y, 0.0f }; // Bottom-Right
		v1.Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		v1.TexCoords = { 1.0f, 0.0f };
		v1.TexID = 0.0f;

		Vertex v2;
		v2.Position = { x + width, y + height, 0.0f }; // Top-Right
		v2.Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		v2.TexCoords = { 1.0f, 1.0f };
		v2.TexID = 0.0f;

		Vertex v3;
		v3.Position = { x, y + height, 0.0f }; // Top-Left
		v3.Color = { 0.8f, 0.8f, 0.2f, 1.0f };
		v3.TexCoords = { 0.0f, 1.0f };
		v3.TexID = 0.0f;

		return { v0, v1, v2, v3 };
	}

	const size_t MaxQuadCount = 1000;
	const size_t MaxVertexCount = MaxQuadCount * 4;
	const size_t MaxIndexCount = MaxQuadCount * 6;

	TestBatchRendering::TestBatchRendering()
		:m_Shader("res/shaders/BatchRendering.shader"),
		m_VB(nullptr, sizeof(Vertex) * 1000),
		m_IB(nullptr, MaxIndexCount),
		m_Texture_1("res/textures/logo.png"),
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

		m_Proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
		m_View = glm::mat4(1.0f);
		m_Model = glm::mat4(1.0f);

		m_Shader.Bind();

		m_Texture_1.Bind(0);
		m_Texture_2.Bind(1);

		int samplers[] = { 0, 1 };
		m_Shader.setUniform1iv("u_Texture", 2, samplers);

		m_Shader.Unbind();
	}

	TestBatchRendering::~TestBatchRendering()
	{

	}

	void TestBatchRendering::OnUpdate(float deltaTime)
	{

		m_Renderer.Clear();

		//set dynamic vertex buuffer
		auto q1 = CreateQuad(m_QuadPosition[0], m_QuadPosition[1]);
		auto q2 = CreateQuad(0.04f, -0.9f);
		auto q3 = CreateQuad(-0.9f, 0.5f);


		Vertex vertices[12];
		memcpy(vertices, q1.data(), q1.size() * sizeof(Vertex) );
		memcpy(vertices + q1.size(), q2.data(), q2.size() * sizeof(Vertex));
		memcpy(vertices + 2*q2.size(), q3.data(), q3.size() * sizeof(Vertex));

		m_VA.Bind();
		GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices));


		m_Shader.Bind();

		glm::mat4 mvp = m_Proj * m_View * m_Model;

		m_Shader.setUniformMat4f("u_MVP", mvp);

		m_Shader.Unbind();

	}

	void TestBatchRendering::OnRender()
	{
		m_Renderer.Draw(m_VA, m_IB, m_Shader);
	}

	void TestBatchRendering::OnImGuiRender()
	{
	
		ImGui::Begin("Controls");	
		ImGui::DragFloat2("Quad Position", m_QuadPosition, 0.1f);
		ImGui::End();

	}

	void TestBatchRendering::OnEvent(int key, int action)
	{
		if (key == GLFW_KEY_W && action == GLFW_PRESS)
		{
			std::cout << "W pressed \n";
		}

		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			std::cout << "Escape pressed \n";
		}
	}

}