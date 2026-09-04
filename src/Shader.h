#pragma once


#include <string>
#include<unordered_map>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

struct ShaderProgramSource
{
	std::string VertexSource;
	std::string FragmentSource;
};

class Shader
{
private:

	std::string m_FilePath;
	unsigned int m_RendererID;
	mutable std::unordered_map <std::string, int> m_UniformLocationCache;

public:
	Shader(const std::string& filepath);
	~Shader();

	void Bind() const;
	void Unbind() const;

	//set uniforms

	void setUniform1i(const std::string& name, int value);
	void setUniform1iv(const std::string& name, unsigned int count, const int* values);
	void setUniform1f(const std::string& name, float value);
	void setUniform4f(const std::string& name, float v0, float v1, float v2, float v3);
	void setUniformMat4f(const std::string& name, const glm::mat4& matrix);


private:


	ShaderProgramSource ParseShader(const std::string& filepath);

	unsigned int CreateShader(const std::string& vertexShader, const std::string& fragmentShader);
	unsigned int CompileShader(unsigned int type, const std::string& source);
	int GetUniformLocation(const std::string& name) const;
};