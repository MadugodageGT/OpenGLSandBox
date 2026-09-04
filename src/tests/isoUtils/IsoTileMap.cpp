#include "IsoTileMap.h";

#include <cmath>

IsoTileMap::IsoTileMap(float windowWidth, float windowHeight, unsigned int rows, unsigned int collumns)
	:m_Rows(rows),
	m_Collumns(collumns)
{

	float x = windowWidth / 2;
	float gridSize = std::sqrtf(windowWidth * windowWidth + windowHeight * windowHeight) * m_TileHeight / 4;
	float y = (windowHeight + gridSize)/2;

	Vertex* buffer = vertices.data();
	for (int i = 0; i < 20; i++)
	{
		for (int j = 0; j < 20; j++)
		{
			buffer = CreateQuad(buffer, x + (16.0f * j) - (16.0f * i), y - (8.0f * j) - (8.0f * i), 0);
		}
	}
}

IsoTileMap::~IsoTileMap()
{
}

Vertex* IsoTileMap::CreateQuad(Vertex* target, float x, float y, unsigned int slot)
{

	target->Position = { x, y, 0.0f }; // Bottom-Left
	target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
	target->TexCoords = { 0.0f + (slot * 0.25), 0.0f };
	target->TexID = 0.0f;
	target++;

	target->Position = { x + m_TileWidth, y, 0.0f }; // Bottom-Right
	target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
	target->TexCoords = { 0.25f + (slot * 0.25), 0.0f };
	target->TexID = 0.0f;
	target++;


	target->Position = { x + m_TileWidth, y + m_TileHeight, 0.0f }; // Top-Right
	target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
	target->TexCoords = { 0.25f + (slot * 0.25), 1.0f };
	target->TexID = 0.0f;
	target++;

	target->Position = { x, y + m_TileHeight, 0.0f }; // Top-Left
	target->Color = { 0.8f, 0.8f, 0.2f, 1.0f };
	target->TexCoords = { 0.0f + (slot * 0.25), 1.0f };
	target->TexID = 0.0f;
	target++;

	return target;
}
