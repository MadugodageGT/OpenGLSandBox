#pragma once

#include <array>

#include "glm/glm.hpp"

struct Vertex {
	glm::vec3 Position;
	glm::vec4 Color;
	glm::vec2 TexCoords;
	float TexID;
};

const size_t MaxQuadCount = 1000;
const size_t MaxVertexCount = MaxQuadCount * 4;
const size_t MaxIndexCount = MaxQuadCount * 6;



class IsoTileMap {
private:
	unsigned int m_Rows;
	unsigned int m_Collumns;

	float m_TileWidth = 32;
	float m_TileHeight = 32;

	std::array <Vertex, MaxVertexCount> vertices;

public:
	IsoTileMap(float windowWidth, float windowHeight, unsigned int rows, unsigned int collumns);
	~IsoTileMap();

private:

	Vertex* CreateQuad(Vertex* target, float x, float y, unsigned int slot);
};

