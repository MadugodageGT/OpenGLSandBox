#include "SurfaceMesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "tinygltf/tiny_gltf_v3.h"

namespace {

int FindAttribute(const tg3_primitive& primitive, const char* name)
{
    for (uint32_t i = 0; i < primitive.attributes_count; ++i)
    {
        if (tg3_str_equals_cstr(primitive.attributes[i].key, name))
            return primitive.attributes[i].value;
    }
    return -1;
}

// Frees the parsed model on every exit path (including exceptions).
struct ModelGuard
{
    tg3_model* model;
    tg3_error_stack* errors;
    ~ModelGuard()
    {
        tg3_model_free(model);
        tg3_error_stack_free(errors);
    }
};

// Returns a pointer to element 0 of the accessor and its byte stride; validates the range.
const uint8_t* AccessorBase(const tg3_model& m, const tg3_accessor& a, size_t elementSize, int32_t& stride)
{
    if (a.buffer_view < 0 || a.sparse.is_sparse)
        throw std::runtime_error("Unsupported glTF accessor (sparse or without buffer view)");

    const tg3_buffer_view& view = m.buffer_views[a.buffer_view];
    const tg3_buffer& buffer = m.buffers[view.buffer];

    stride = tg3_accessor_byte_stride(&a, &view);
    if (stride <= 0)
        throw std::runtime_error("Invalid glTF accessor stride");

    const uint64_t start = view.byte_offset + a.byte_offset;
    const uint64_t end = start + (a.count > 0 ? (a.count - 1) * static_cast<uint64_t>(stride) + elementSize : 0);
    if (end > buffer.data.count)
        throw std::runtime_error("glTF accessor reads past the end of its buffer");

    return buffer.data.data + start;
}

uint32_t ReadIndex(const uint8_t* p, int componentType)
{
    switch (componentType)
    {
    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:  return *p;
    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: { uint16_t v; std::memcpy(&v, p, sizeof v); return v; }
    case TG3_COMPONENT_TYPE_UNSIGNED_INT:   { uint32_t v; std::memcpy(&v, p, sizeof v); return v; }
    default: throw std::runtime_error("Unsupported glTF index type");
    }
}

size_t IndexSize(int componentType)
{
    switch (componentType)
    {
    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:  return 1;
    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
    case TG3_COMPONENT_TYPE_UNSIGNED_INT:   return 4;
    default: throw std::runtime_error("Unsupported glTF index type");
    }
}

bool IsFloatVec3(const tg3_accessor& a)
{
    return a.component_type == TG3_COMPONENT_TYPE_FLOAT && a.type == TG3_TYPE_VEC3 && !a.sparse.is_sparse;
}

} // namespace

SurfaceMesh::SurfaceMesh(const std::string& glbPath)
    : m_Center(0.0f), m_Radius(1.0f), m_Vao(0), m_Vbo(0), m_Ebo(0), m_IndexCount(0)
{
    tg3_model model{};
    tg3_error_stack errors{};
    tg3_parse_options options{};

    tg3_error_stack_init(&errors);
    tg3_parse_options_init(&options);

    const tg3_error_code result = tg3_parse_file(&model, &errors, glbPath.c_str(),
                                                 static_cast<uint32_t>(glbPath.size()), &options);
    if (result != TG3_OK)
    {
        tg3_error_stack_free(&errors);
        throw std::runtime_error("Failed to load model: " + glbPath);
    }

    ModelGuard guard = { &model, &errors };

    bool allHaveNormals = true;

    for (uint32_t mi = 0; mi < model.meshes_count; ++mi)
    {
        const tg3_mesh& mesh = model.meshes[mi];
        for (uint32_t pi = 0; pi < mesh.primitives_count; ++pi)
        {
            const tg3_primitive& prim = mesh.primitives[pi];
            if (prim.mode != -1 && prim.mode != TG3_MODE_TRIANGLES)
                continue;

            const int posIndex = FindAttribute(prim, "POSITION");
            if (posIndex < 0)
                continue;
            const tg3_accessor& posAcc = model.accessors[posIndex];
            if (!IsFloatVec3(posAcc))
                throw std::runtime_error("Expected a non-sparse FLOAT VEC3 POSITION accessor");

            int32_t posStride = 0;
            const uint8_t* posData = AccessorBase(model, posAcc, 12, posStride);

            const uint32_t base = static_cast<uint32_t>(m_Positions.size());
            const size_t vertexCount = static_cast<size_t>(posAcc.count);

            for (size_t v = 0; v < vertexCount; ++v)
            {
                glm::vec3 p;
                std::memcpy(&p, posData + v * static_cast<size_t>(posStride), sizeof p);
                m_Positions.push_back(p);
            }

            // Normals (optional).
            const int nrmIndex = FindAttribute(prim, "NORMAL");
            bool haveNormals = false;
            if (nrmIndex >= 0)
            {
                const tg3_accessor& nAcc = model.accessors[nrmIndex];
                if (IsFloatVec3(nAcc) && static_cast<size_t>(nAcc.count) == vertexCount)
                {
                    int32_t nStride = 0;
                    const uint8_t* nData = AccessorBase(model, nAcc, 12, nStride);
                    for (size_t v = 0; v < vertexCount; ++v)
                    {
                        glm::vec3 n;
                        std::memcpy(&n, nData + v * static_cast<size_t>(nStride), sizeof n);
                        m_Normals.push_back(n);
                    }
                    haveNormals = true;
                }
            }
            if (!haveNormals)
            {
                allHaveNormals = false;
                m_Normals.resize(m_Positions.size(), glm::vec3(0.0f));
            }

            // Indices (or an implicit 0..n-1 sequence).
            if (prim.indices >= 0)
            {
                const tg3_accessor& iAcc = model.accessors[prim.indices];
                int32_t iStride = 0;
                const uint8_t* iData = AccessorBase(model, iAcc, IndexSize(iAcc.component_type), iStride);
                const size_t count = static_cast<size_t>(iAcc.count) / 3 * 3;
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t idx = ReadIndex(iData + i * static_cast<size_t>(iStride), iAcc.component_type);
                    if (idx >= vertexCount)
                        throw std::runtime_error("glTF index out of range");
                    m_Indices.push_back(base + idx);
                }
            }
            else
            {
                const size_t count = vertexCount / 3 * 3;
                for (size_t i = 0; i < count; ++i)
                    m_Indices.push_back(base + static_cast<uint32_t>(i));
            }
        }
    }

    if (m_Positions.empty() || m_Indices.empty())
        throw std::runtime_error("The model contains no triangle mesh");

    if (!allHaveNormals)
        ComputeSmoothNormals();

    // Bounding sphere (centre of the box, farthest vertex).
    glm::vec3 lo = m_Positions[0];
    glm::vec3 hi = m_Positions[0];
    for (size_t i = 1; i < m_Positions.size(); ++i)
    {
        lo = glm::min(lo, m_Positions[i]);
        hi = glm::max(hi, m_Positions[i]);
    }
    m_Center = 0.5f * (lo + hi);
    float r2 = 0.0f;
    for (size_t i = 0; i < m_Positions.size(); ++i)
    {
        const glm::vec3 d = m_Positions[i] - m_Center;
        r2 = (std::max)(r2, glm::dot(d, d));
    }
    m_Radius = (std::max)(std::sqrt(r2), 1e-4f);

    UploadToGpu();
}

SurfaceMesh::~SurfaceMesh()
{
    if (m_Ebo) glDeleteBuffers(1, &m_Ebo);
    if (m_Vbo) glDeleteBuffers(1, &m_Vbo);
    if (m_Vao) glDeleteVertexArrays(1, &m_Vao);
}

void SurfaceMesh::ComputeSmoothNormals()
{
    m_Normals.assign(m_Positions.size(), glm::vec3(0.0f));
    for (size_t i = 0; i + 2 < m_Indices.size(); i += 3)
    {
        const uint32_t a = m_Indices[i], b = m_Indices[i + 1], c = m_Indices[i + 2];
        // Area-weighted: the cross product's length is twice the triangle area.
        const glm::vec3 n = glm::cross(m_Positions[b] - m_Positions[a], m_Positions[c] - m_Positions[a]);
        m_Normals[a] += n;
        m_Normals[b] += n;
        m_Normals[c] += n;
    }
    for (size_t i = 0; i < m_Normals.size(); ++i)
    {
        const float len = glm::length(m_Normals[i]);
        m_Normals[i] = len > 1e-12f ? m_Normals[i] / len : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

void SurfaceMesh::UploadToGpu()
{
    std::vector<float> interleaved;
    interleaved.reserve(m_Positions.size() * 6);
    for (size_t i = 0; i < m_Positions.size(); ++i)
    {
        interleaved.push_back(m_Positions[i].x);
        interleaved.push_back(m_Positions[i].y);
        interleaved.push_back(m_Positions[i].z);
        interleaved.push_back(m_Normals[i].x);
        interleaved.push_back(m_Normals[i].y);
        interleaved.push_back(m_Normals[i].z);
    }

    glGenVertexArrays(1, &m_Vao);
    glBindVertexArray(m_Vao);

    glGenBuffers(1, &m_Vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_Vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                 interleaved.data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));

    glGenBuffers(1, &m_Ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_Indices.size() * sizeof(uint32_t)),
                 m_Indices.data(), GL_STATIC_DRAW);
    m_IndexCount = static_cast<GLsizei>(m_Indices.size());

    glBindVertexArray(0);
}

void SurfaceMesh::Draw() const
{
    glBindVertexArray(m_Vao);
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
