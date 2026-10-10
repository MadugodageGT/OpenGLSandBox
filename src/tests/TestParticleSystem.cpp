#include "TestParticleSystem.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>




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
	};

	static unsigned int s_Indices[] =
	{
		0,1,2,//left
		2,3,0,

		4,5,6,//right
		6,7,4
	};

	struct Particle {
		
		vec3 Position;
		float Velosity;
		float Life;

	};




	static std::array<Vertex, 4> CreateQuad(float x, float y)
	{
		float width = 0.03f;
		float height = 0.03f;

		Vertex v0;
		v0.Position = { x, y, 0.0f }; // Bottom-Left
		v0.Color = { 0.8f, 0.8f, 0.2f, 1.0f };


		Vertex v1;
		v1.Position = { x + width, y, 0.0f }; // Bottom-Right
		v1.Color = { 0.8f, 0.8f, 0.2f, 1.0f };


		Vertex v2;
		v2.Position = { x + width, y + height, 0.0f }; // Top-Right
		v2.Color = { 0.8f, 0.8f, 0.2f, 1.0f };


		Vertex v3;
		v3.Position = { x, y + height, 0.0f }; // Top-Left
		v3.Color = { 0.8f, 0.8f, 0.2f, 1.0f };

		return { v0, v1, v2, v3 };
	}


	const size_t MaxQuadCount = 1000;
	const size_t MaxVertexCount = MaxQuadCount * 4;
	const size_t MaxIndexCount = MaxQuadCount * 6;

	test::TestParticleSystem::TestParticleSystem()
		: m_Shader("res/shaders/ParticleSystem.shader"),
		m_VB(nullptr, sizeof(Vertex) * 1000),
		m_IB(nullptr, MaxIndexCount)

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

		m_VA.AddBuffer(m_VB, layout);
		m_IB.Update(indices, MaxIndexCount);

		m_Proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
		m_View = glm::mat4(1.0f);
		m_Model = glm::mat4(1.0f);

		m_Particle.Position = { 0.0f, 0.0f, 0.0f };
		m_Particle.Velocity = 1.0f;
		m_Particle.Life = 1.0f;

	}

	test::TestParticleSystem::~TestParticleSystem()
	{
	}



	void test::TestParticleSystem::OnUpdate(float deltaTime)
	{
		
		
		m_Renderer.Clear();

		std::vector<Vertex> vertices;


		if(!m_IsAlive)
		{
			m_Time = 0.0f;
			m_Particle.Position = { (float)m_PosX, (float)m_PosY, 0.0f };
			m_Particle.Velocity = 0.03f;
			m_Particle.Life = 300.0f;
			m_IsAlive = !m_IsAlive;
		}

		else
		{
			m_Particle.Position.x += m_Particle.Velocity * std::cos(0.523599f) * deltaTime;
			m_Particle.Position.y += m_Particle.Velocity * std::sin(0.523599f) * deltaTime;
			m_Particle.Life -= deltaTime;
			if (m_Particle.Life <= 0.0f) {
				m_IsAlive = false;
			}
		}


		auto q1 = CreateQuad(m_Particle.Position.x, m_Particle.Position.y);

		vertices.insert(vertices.end(), q1.begin(),q1.end());


		m_VA.Bind();
		GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data()));

		m_Shader.Bind();

		glm::mat4 mvp = m_Proj * m_View * m_Model;

		m_Shader.setUniformMat4f("u_MVP", mvp);

		m_Shader.Unbind();

	}

	void test::TestParticleSystem::OnRender()
	{
		m_Renderer.Draw(m_VA, m_IB, m_Shader);
	}

	void test::TestParticleSystem::OnImGuiRender()
	{
	}

	void test::TestParticleSystem::OnEvent(int key, int action)
	{
	
	}
	void TestParticleSystem::OnMouseButtonEvent(int button, int action, double mouseX, double mouseY)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		{
			m_IsAdding = true;

			m_PosX = (mouseX/1024.0f) * 2.0f - 1.0f;
			m_PosY = 1.0f - (mouseY/720.0f) * 2.0f ;

		}
	}
}