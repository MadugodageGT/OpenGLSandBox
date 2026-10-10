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




	class TestParticleSystem : public Test
	{
	public:
		TestParticleSystem();
		~TestParticleSystem();

		void OnUpdate(float deltaTime) override;
		void OnRender() override;
		void OnImGuiRender() override;

		void OnEvent(int key, int action) override;

		void OnMouseButtonEvent(int button, int action, double mouseX, double mouseY) override;

	private:

		struct Particle {
			glm::vec3 Position;
			float Velocity;
			float Life;
		};


		Renderer m_Renderer;
		Shader m_Shader;

		VertexArray m_VA;
		VertexBuffer m_VB;
		IndexBuffer m_IB;

		bool m_IsAdding = false;
		bool m_IsAlive = false;

		glm::mat4 m_Proj;
		glm::mat4 m_View;
		glm::mat4 m_Model;

		double m_PosX = 0.0f;
		double m_PosY = 0.0f;

		float m_Time = 0.0f;

		Particle m_Particle;


	};


}