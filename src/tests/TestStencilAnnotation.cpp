#include <memory>

#include "TestStencilAnnotation.h"
#include "imgui/ImGui.h"

#include <array>

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


    static std::array<vec3, 8> CreateCube(float x, float y, float z)
    {
        float width = 0.5f;
        float height = 0.5f;
        float depth = 0.5f;

        return
        {
            vec3{ x,         y,          z + depth }, 
            vec3{ x + width, y,          z + depth }, 
            vec3{ x + width, y + height, z + depth }, 
            vec3{ x,         y + height, z + depth }, 

            vec3{ x,         y,          z },         
            vec3{ x + width, y,          z },         
            vec3{ x + width, y + height, z },         
            vec3{ x,         y + height, z }          
        };
    }


    const size_t MaxCubeCount = 1000;
    const size_t MaxVertexCount = MaxCubeCount * 8;
    const size_t MaxIndexCount = MaxCubeCount * 36;

    TestStencilAnnotation::TestStencilAnnotation()
        : m_VBO(nullptr,sizeof(vec3) * MaxCubeCount),
		m_EBO(nullptr, MaxIndexCount),
        m_Shader("res/shaders/Mesh.shader"),
        m_OutlineShader("res/shaders/SingleColor.shader"),
        m_ModelPosition(0.0f),
        m_ModelRotation(0.0f),
        m_CubePosition(0.0f),
        m_CubeRotation(0.0f)
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

		// ---- GL buffers for loaded mesh ----
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
        
        //GL Buffers for Cube

        //unsigned int cubeIndices[MaxIndexCount];
        //unsigned int offset = 0;

        //for (size_t cube = 0; cube < MaxCubeCount; ++cube)
        //{
        //    const unsigned int vertexOffset = static_cast<unsigned int>(cube * 8);
        //    const size_t indexOffset = cube * 36;

        //    for (size_t i = 0; i < 36; ++i)
        //        cubeIndices[indexOffset + i] = cubeIndices[i] + vertexOffset;
        //}


        static constexpr unsigned int CubeIndices[36] =
        {
            0, 1, 2,  2, 3, 0, // front
            4, 7, 6,  6, 5, 4, // back
            0, 3, 7,  7, 4, 0, // left
            1, 5, 6,  6, 2, 1, // right
            0, 4, 5,  5, 1, 0, // bottom
            3, 2, 6,  6, 7, 3  // top
        };

        m_VAO.Bind();

        VertexBufferLayout layout;
        layout.Push<float>(3);

		m_VAO.AddBuffer(m_VBO, layout);

		m_EBO.Update(CubeIndices, 36);

        // camera metrices
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

    TestStencilAnnotation::~TestStencilAnnotation()
    {
        glDeleteBuffers(1, &m_IndexEBO);
        glDeleteBuffers(1, &m_PositionVBO);
        glDeleteVertexArrays(1, &m_MeshVAO);
        glDeleteBuffers(1, &m_TexCoordVBO);
    }

    void TestStencilAnnotation::OnUpdate(float deltaTime)
    {
        m_Renderer.Clear();
    }

    void TestStencilAnnotation::OnRender()
    {


        GLCall(glEnable(GL_DEPTH_TEST));
        GLCall(glEnable(GL_STENCIL_TEST));
        GLCall(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));


        // pass 1 : draw object
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);

        m_Model = glm::mat4(1.0f);
        m_Model = glm::translate(m_Model, m_ModelPosition);
        m_Model = glm::rotate(
            m_Model,
            glm::radians(m_ModelRotation.y),
            glm::vec3(0.0f, 1.0f, 0.0f));
        m_Model = glm::rotate(
            m_Model,
            glm::radians(m_ModelRotation.x),
            glm::vec3(1.0f, 0.0f, 0.0f));
        m_Model = glm::rotate(
            m_Model,
            glm::radians(m_ModelRotation.z),
            glm::vec3(0.0f, 0.0f, 1.0f));


        const glm::mat4 mvp = m_Proj * m_View * m_Model;


        m_Texture->Bind(0);
        m_Shader.Bind();
        m_Shader.setUniformMat4f("u_MVP", mvp);
        m_Shader.setUniform1i("u_Texture", 0);
        m_Shader.setUniform4f("u_Color", 1.0f, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(m_MeshVAO);
        glDrawElements(GL_TRIANGLES, m_IndexCount, m_IndexType, nullptr);
        glBindVertexArray(0);



        // Pass 2
         
        const float outlineScale = 1.0f; 

        glm::mat4 outlineModel = glm::mat4(1.0f);
        outlineModel = glm::translate(outlineModel, m_CubePosition);
        outlineModel = glm::rotate(outlineModel, glm::radians(m_CubeRotation), glm::vec3(0.0f, 1.0f, 0.0f));
        outlineModel = glm::scale(outlineModel, glm::vec3(outlineScale)); 

        const glm::mat4 outlineMVP = m_Proj * m_View * outlineModel;




        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilMask(0x00);
        glDisable(GL_DEPTH_TEST);


		auto cube1 = CreateCube(m_CubePosition.x, m_CubePosition.y, m_CubePosition.z);

		std::vector<vec3> cubeVertices;
		cubeVertices.insert(cubeVertices.end(), cube1.begin(), cube1.end());


        m_OutlineShader.Bind();
        m_OutlineShader.setUniformMat4f("u_MVP", mvp);

        m_VAO.Bind();
        glBufferSubData(GL_ARRAY_BUFFER, 0, cubeVertices.size() * sizeof(vec3), cubeVertices.data());

        m_Renderer.Draw(m_VAO, m_EBO, m_OutlineShader);

        m_VAO.Unbind();

        //Restore the state
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);

    }

    void TestStencilAnnotation::OnImGuiRender()
    {
        {
            ImGui::SliderFloat("X", &m_ModelPosition.x, -1.0f, 1.0f);
            ImGui::SliderFloat("Y", &m_ModelPosition.y, -1.0f, 1.0f);
            ImGui::SliderFloat("Z", &m_ModelPosition.z, -1.0f, 1.0f);
            ImGui::SliderFloat("Rotation X", &m_ModelRotation.x, 0.0f, 360.0f);
            ImGui::SliderFloat("Rotation y", &m_ModelRotation.y, 0.0f, 360.0f);
            ImGui::SliderFloat("Rotation z", &m_ModelRotation.z, 0.0f, 360.0f);
		}
        {      
            ImGui::SliderFloat("cube X", &m_CubePosition.x, -1.0f, 1.0f);
            ImGui::SliderFloat("cube Y", &m_CubePosition.y, -1.0f, 1.0f);
            ImGui::SliderFloat("cube Z", &m_CubePosition.z, -1.0f, 1.0f);
        }
  
    }

}

