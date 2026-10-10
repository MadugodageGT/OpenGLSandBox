#include <memory>

#include "TestLoadingMesh.h"
#include "imgui/ImGui.h"

unsigned int m_TexCoordVBO = 0;
std::unique_ptr<Texture> m_Texture;

namespace
{
	int FindAttribute(const tg3_primitive& primitive, const char* name)
	{
		for (uint32_t i = 0; i < primitive.attributes_count; ++i)
		{
			if (tg3_str_equals_cstr(primitive.attributes[i].key, name))
				return primitive.attributes[i].value;
		}

		return -1;
	}

	GLenum ToGLIndexType(int componentType)
	{
		switch (componentType)
		{
		case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:  return GL_UNSIGNED_BYTE;
		case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
		case TG3_COMPONENT_TYPE_UNSIGNED_INT:   return GL_UNSIGNED_INT;
		default:
			throw std::runtime_error("Unsupported glTF index format");
		}
	}
}




namespace test {


	static GLenum toGlDrawMode(int mode) {
		switch (mode == -1 ? TG3_MODE_TRIANGLES : mode) {
		case TG3_MODE_POINTS:         return GL_POINTS;
		case TG3_MODE_LINE:           return GL_LINES;
		case TG3_MODE_LINE_LOOP:      return GL_LINE_LOOP;
		case TG3_MODE_LINE_STRIP:     return GL_LINE_STRIP;
		case TG3_MODE_TRIANGLES:      return GL_TRIANGLES;
		case TG3_MODE_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
		case TG3_MODE_TRIANGLE_FAN:   return GL_TRIANGLE_FAN;
		default: throw std::runtime_error("Unsupported glTF primitive mode");
		}
	}


    TestLoadingMesh::TestLoadingMesh()
        : m_Shader("res/shaders/Mesh.shader"),
        m_OutlineShader("res/shaders/SingleColor.shader"),
        m_ModelPosition(0.0f),
        m_ModelRotation(0.0f)
    {
        tg3_model model{};
        tg3_error_stack errors{};
        tg3_parse_options options{};

        tg3_error_stack_init(&errors);
        tg3_parse_options_init(&options);

        const char* path = "res/models/world.glb";

        const tg3_error_code result = tg3_parse_file(
            &model,
            &errors,
            path,
            static_cast<uint32_t>(strlen(path)),
            &options);

        if (result != TG3_OK)
        {
            tg3_error_stack_free(&errors);
            throw std::runtime_error("Failed to load model");
        }

        if (model.meshes_count == 0 || model.meshes[0].primitives_count == 0)
        {
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
            throw std::runtime_error("The model contains no mesh primitives");
        }

        const tg3_primitive& primitive = model.meshes[0].primitives[0];

        const int positionAccessorIndex = FindAttribute(primitive, "POSITION");

        if (positionAccessorIndex < 0 || primitive.indices < 0)
        {
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
            throw std::runtime_error("This first loader requires POSITION and indices");
        }

        const tg3_accessor& position = model.accessors[positionAccessorIndex];
        const tg3_buffer_view& positionView = model.buffer_views[position.buffer_view];
        const tg3_buffer& positionBuffer = model.buffers[positionView.buffer];

        if (position.component_type != TG3_COMPONENT_TYPE_FLOAT ||
            position.type != TG3_TYPE_VEC3 ||
            position.sparse.is_sparse)
        {
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
            throw std::runtime_error("Expected a non-sparse FLOAT VEC3 POSITION accessor");
        }

        const tg3_accessor& indices = model.accessors[primitive.indices];
        const tg3_buffer_view& indexView = model.buffer_views[indices.buffer_view];
        const tg3_buffer& indexBuffer = model.buffers[indexView.buffer];

                // ---- TEXCOORD_0 ----
        const int uvAccessorIndex = FindAttribute(primitive, "TEXCOORD_0");
        if (uvAccessorIndex < 0)
        {
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
            throw std::runtime_error("Model has no TEXCOORD_0");
        }

        const tg3_accessor& uv = model.accessors[uvAccessorIndex];
        const tg3_buffer_view& uvView = model.buffer_views[uv.buffer_view];
        const tg3_buffer& uvBuffer = model.buffers[uvView.buffer];

        if (uv.component_type != TG3_COMPONENT_TYPE_FLOAT || uv.type != TG3_TYPE_VEC2)
        {
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
            throw std::runtime_error("Expected FLOAT VEC2 TEXCOORD_0");
        }

        // ---- Base color texture (embedded in the .glb) ----
        if (primitive.material >= 0)
        {
            const tg3_material& material = model.materials[primitive.material];
            const int texIndex = material.pbr_metallic_roughness.base_color_texture.index;

            if (texIndex >= 0)
            {
                const tg3_texture& tex = model.textures[texIndex];
                if (tex.source >= 0)
                {
                    const tg3_image& image = model.images[tex.source];
                    if (image.buffer_view >= 0)
                    {
                        const tg3_buffer_view& imgView = model.buffer_views[image.buffer_view];
                        const tg3_buffer& imgBuffer = model.buffers[imgView.buffer];
                        m_Texture = std::make_unique<Texture>(
                            imgBuffer.data.data + imgView.byte_offset,
                            static_cast<int>(imgView.byte_length));
                    }
                }
            }
        }

        if (!m_Texture)
        {
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
            throw std::runtime_error("Model has no embedded base color texture");
        }

        // ---- GL buffers ----
        glGenVertexArrays(1, &m_MeshVAO);
        glBindVertexArray(m_MeshVAO);

        glGenBuffers(1, &m_PositionVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_PositionVBO);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(positionView.byte_length),
            positionBuffer.data.data + positionView.byte_offset,
            GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            tg3_accessor_byte_stride(&position, &positionView),
            reinterpret_cast<void*>(static_cast<uintptr_t>(position.byte_offset)));

        glGenBuffers(1, &m_TexCoordVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_TexCoordVBO);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(uvView.byte_length),
            uvBuffer.data.data + uvView.byte_offset,
            GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            tg3_accessor_byte_stride(&uv, &uvView),
            reinterpret_cast<void*>(static_cast<uintptr_t>(uv.byte_offset)));

        glGenBuffers(1, &m_IndexEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indexView.byte_length - indices.byte_offset),
            indexBuffer.data.data + indexView.byte_offset + indices.byte_offset,
            GL_STATIC_DRAW);

        m_IndexCount = static_cast<GLsizei>(indices.count);
        m_IndexType = ToGLIndexType(indices.component_type);

        glBindVertexArray(0);

        tg3_model_free(&model);
        tg3_error_stack_free(&errors);

        m_Proj = glm::perspective(
            glm::radians(45.0f),
            1024.0f / 720.0f,
            0.1f,
            100.0f);

        m_View = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.0f),
            glm::vec3(0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
    }

	TestLoadingMesh::~TestLoadingMesh()
	{
        glDeleteBuffers(1, &m_IndexEBO);
        glDeleteBuffers(1, &m_PositionVBO);
        glDeleteVertexArrays(1, &m_MeshVAO);
        glDeleteBuffers(1, &m_TexCoordVBO);
	}

	void TestLoadingMesh::OnUpdate(float deltaTime)
	{
		m_Renderer.Clear();
	}

    void TestLoadingMesh::OnRender()
    {
        GLCall(glEnable(GL_DEPTH_TEST));
        GLCall(glEnable(GL_STENCIL_TEST));
        GLCall(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));

        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);

        m_Model = glm::mat4(1.0f);
        m_Model = glm::translate(m_Model, m_ModelPosition);
        m_Model = glm::rotate(
            m_Model,
            glm::radians(m_ModelRotation),
            glm::vec3(0.0f, 1.0f, 0.0f));

        const glm::mat4 mvp = m_Proj * m_View * m_Model;


        m_Texture->Bind(0);
        m_Shader.Bind();
        m_Shader.setUniformMat4f("u_MVP", mvp);
        m_Shader.setUniform1i("u_Texture", 0);
        m_Shader.setUniform4f("u_Color", 1.0f, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(m_MeshVAO);
        glDrawElements(GL_TRIANGLES, m_IndexCount, m_IndexType, nullptr);
		glBindVertexArray(0);


        const float outlineScale = 1.02f; // ~5% bigger; expose in ImGui to tweak

        glm::mat4 outlineModel = glm::mat4(1.0f);
        outlineModel = glm::translate(outlineModel, m_ModelPosition);
        outlineModel = glm::rotate(outlineModel, glm::radians(m_ModelRotation), glm::vec3(0.0f, 1.0f, 0.0f));
        outlineModel = glm::scale(outlineModel, glm::vec3(outlineScale)); // applied first to the vertices

        const glm::mat4 outlineMVP = m_Proj * m_View * outlineModel;

        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilMask(0x00);
        GLCall(glDisable(GL_DEPTH_TEST));

		m_OutlineShader.Bind();
		m_OutlineShader.setUniformMat4f("u_MVP", outlineMVP);
        m_OutlineShader.setUniform1i("u_Texture", 0);
        m_OutlineShader.setUniform4f("u_Color", 1.0f, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(m_MeshVAO);
        glDrawElements(GL_TRIANGLES, m_IndexCount, m_IndexType, nullptr);
        glBindVertexArray(0);

        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);

    }

	void TestLoadingMesh::OnImGuiRender()
	{
		{
			ImGui::SliderFloat("X", &m_ModelPosition.x, -1.0f, 1.0f);
			ImGui::SliderFloat("Y", &m_ModelPosition.y, -1.0f, 1.0f);
			ImGui::SliderFloat("Z", &m_ModelPosition.z, -1.0f, 1.0f);
		}
		{
			ImGui::SliderFloat("Rotation", &m_ModelRotation, 0.0f, 360.0f);

		}
	
	}

}

