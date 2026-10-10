
#include "Texture.h"
#include "stb_image/stb_image.h"
#include <stdexcept>





Texture::Texture(const std::string& path)
	: m_RendererID(0), m_FilePath(path), m_LocalBuffer(nullptr), m_Width(0), m_Height(0), m_BPP(0)
{
	stbi_set_flip_vertically_on_load(1);
	m_LocalBuffer = stbi_load(path.c_str(), &m_Width, &m_Height, &m_BPP, 4);
	Upload(m_LocalBuffer, GL_CLAMP_TO_EDGE);
	if (m_LocalBuffer) stbi_image_free(m_LocalBuffer);
}

Texture::Texture(const unsigned char* encodedData, int size)
	: m_RendererID(0), m_LocalBuffer(nullptr), m_Width(0), m_Height(0), m_BPP(0)
{
	stbi_set_flip_vertically_on_load(0); // glTF UVs are already top-left origin
	m_LocalBuffer = stbi_load_from_memory(encodedData, size, &m_Width, &m_Height, &m_BPP, 4);
	if (!m_LocalBuffer)
		throw std::runtime_error("Failed to decode embedded texture");
	Upload(m_LocalBuffer, GL_REPEAT);
	stbi_image_free(m_LocalBuffer);
}


Texture::~Texture()
{
	GLCall(glDeleteTextures(1, &m_RendererID));

}

void Texture::Bind(unsigned int slot) const
{
	GLCall(glActiveTexture(GL_TEXTURE0 + slot));
	GLCall(glBindTexture(GL_TEXTURE_2D, m_RendererID));

}

void Texture::Unbind() const
{
	GLCall(glBindTexture(GL_TEXTURE_2D, 0));
}

void Texture::Upload(const unsigned char* pixels, GLenum wrap)
{
	GLCall(glGenTextures(1, &m_RendererID));
	GLCall(glBindTexture(GL_TEXTURE_2D, m_RendererID));

	GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
	GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap));
	GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap));

	GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
	GLCall(glGenerateMipmap(GL_TEXTURE_2D));
	GLCall(glBindTexture(GL_TEXTURE_2D, 0));
}