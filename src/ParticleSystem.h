#pragma once

#include <vector>

class ParticleSystem
{
public:
	ParticleSystem();
	~ParticleSystem();

	void Update(float deltaTime);
	void Render();
	void AddParticle(float x, float y, float z);

private:
	struct Particle
	{
		float x, y, z;
		float vx, vy, vz;
		float life;
	};

	std::vector<Particle> particles;
};