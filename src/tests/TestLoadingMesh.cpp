#include "TestLoadingMesh.h"
#include "imgui/ImGui.h"


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
        m_ModelPosition(0.0f),
        m_ModelRotation(0.0f)
    {
        tg3_model model{};
        tg3_error_stack errors{};
        tg3_parse_options options{};

        tg3_error_stack_init(&errors);
        tg3_parse_options_init(&options);

        const char* path = "res/models/suzanne.glb";

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

        glGenVertexArrays(1, &m_MeshVAO);
        glBindVertexArray(m_MeshVAO);

        glGenBuffers(1, &m_PositionVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_PositionVBO);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(positionView.byte_length),
            positionBuffer.data.data + positionView.byte_offset,
            GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            tg3_accessor_byte_stride(&position, &positionView),
            reinterpret_cast<void*>(static_cast<uintptr_t>(position.byte_offset)));

        glGenBuffers(1, &m_IndexEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexEBO);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indexView.byte_length),
            indexBuffer.data.data + indexView.byte_offset,
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
	}

	void TestLoadingMesh::OnUpdate(float deltaTime)
	{
		m_Renderer.Clear();
	}

    void TestLoadingMesh::OnRender()
    {
        GLCall(glEnable(GL_DEPTH_TEST));

        m_Model = glm::mat4(1.0f);
        m_Model = glm::translate(m_Model, m_ModelPosition);
        m_Model = glm::rotate(
            m_Model,
            glm::radians(m_ModelRotation),
            glm::vec3(0.0f, 1.0f, 0.0f));

        const glm::mat4 mvp = m_Proj * m_View * m_Model;

        m_Shader.Bind();
        m_Shader.setUniformMat4f("u_MVP", mvp);
        m_Shader.setUniform4f("u_Color", 0.85f, 0.65f, 0.2f, 1.0f);

        glBindVertexArray(m_MeshVAO);
        glDrawElements(GL_TRIANGLES, m_IndexCount, m_IndexType, nullptr);


        GLCall(glDisable(GL_DEPTH_TEST));
    }

	void TestLoadingMesh::OnImGuiRender()
	{
		{
			ImGui::SliderFloat("X", &m_ModelPosition.x, 0.0f, 960.0f);
			ImGui::SliderFloat("Y", &m_ModelPosition.y, 0.0f, 540.0f);
			ImGui::SliderFloat("Z", &m_ModelPosition.z, -1000.0f, 1000.0f);
		}
		{
			ImGui::SliderFloat("Rotation", &m_ModelRotation, 0.0f, 360.0f);

		}
	
	}





}

