#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <GL/glew.h>

#include "glm/glm.hpp"

// SurfaceMesh - a triangle mesh loaded from a .glb file.
//
// Keeps a CPU copy (positions, normals, indices) next to the GL buffers: the annotation code needs the
// geometry on the CPU (surface area), and the GPU copy is drawn with Draw().
//
// Loading notes: every TRIANGLES primitive of every mesh in the file is merged into one mesh; node
// transforms are ignored (like TestLoadingMesh). Normals come from the file when present, otherwise
// smooth normals are computed.
//
// Draw() only binds the vertex array and issues the draw call; the caller binds the shader.
// Vertex layout: location 0 = position (vec3), location 1 = normal (vec3).

class SurfaceMesh
{
public:
    explicit SurfaceMesh(const std::string& glbPath);   // throws std::runtime_error on failure
    ~SurfaceMesh();

    SurfaceMesh(const SurfaceMesh&) = delete;
    SurfaceMesh& operator=(const SurfaceMesh&) = delete;

    void Draw() const;

    const std::vector<glm::vec3>& Positions() const { return m_Positions; }
    const std::vector<glm::vec3>& Normals() const { return m_Normals; }
    const std::vector<uint32_t>& Indices() const { return m_Indices; }

    size_t TriangleCount() const { return m_Indices.size() / 3; }
    glm::vec3 Center() const { return m_Center; }
    float Radius() const { return m_Radius; }

private:
    void ComputeSmoothNormals();
    void UploadToGpu();

    std::vector<glm::vec3> m_Positions;
    std::vector<glm::vec3> m_Normals;
    std::vector<uint32_t> m_Indices;

    glm::vec3 m_Center;
    float m_Radius;

    GLuint m_Vao;
    GLuint m_Vbo;
    GLuint m_Ebo;
    GLsizei m_IndexCount;
};
