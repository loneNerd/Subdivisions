#pragma once
#ifndef CATMULL_CLARK_SUBDIVITION_MODEL_H_
#define CATMULL_CLARK_SUBDIVITION_MODEL_H_

#include <exception>
#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"

struct aiNode;
struct aiMesh;
struct aiScene;
struct aiMaterial;
enum aiTextureType;

namespace CatmullClarkSubdivision
{
    enum class EModelViewType
    {
        EOriginal,
        ESubdiveded
    };

    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TextureCoordinate;
    };

    struct Texture
    {
        unsigned      Id;
        aiTextureType Type;
        std::string   Path;
    };

    struct Mesh
    {
        std::vector<Vertex>     Vertices;
        std::vector<glm::uvec3> Triangles;
        std::vector<glm::uvec4> Quads;
        std::list<Texture>      Textures;
        unsigned VAO;
        unsigned VBO;
        unsigned EBO;
        bool IsQuads;
    };

    struct VertexRecord
    {
        glm::vec3        Position;
        std::vector<int> Edges;
        std::vector<int> Faces;
    };

    struct EdgeRecord
    {
        int LowVertex  = -1;
        int HighVertex = -1;
        int FaceCount  = 0;
        int Faces[2]   = { -1, -1 };
    };

    struct FaceRecord
    {
        bool IsQuad = false;
        int Vertices[4] = { -1, -1, -1, -1 };
        int Edges[4]    = { -1, -1, -1, -1 };
    };

    class ModelConverter
    {
    public:
        std::vector<VertexRecord> Vertices;
        std::vector<EdgeRecord>   Edges;
        std::vector<FaceRecord>   Faces;

        void moveVertex(int vertexIndex, const glm::vec3& newPosition);
        int getVertexIndex(const glm::vec3& position);
        int getEdgeIndex(const glm::uvec2& vertexPair);

    private:
        std::unordered_map<std::string, int> m_positionToVertex;
        std::unordered_map<std::string, int> m_vertexPairToEdge;
    };

    class Model
    {
    public:
        Model() { }
        ~Model();

        Model(const Model& other)            = delete;
        Model(Model&& other)                 = delete;
        Model& operator=(const Model& other) = delete;
        Model& operator=(Model&& other)      = delete;

        void loadModel(const char* path, glm::mat4 projection, glm::mat4 view);
        void draw(EModelViewType viewType);

        const size_t getVerticesCount(EModelViewType viewType) const;
        const size_t getTrianglesCount(EModelViewType viewType) const;
        const size_t getQuadsCount(EModelViewType viewType) const;
        const bool isQuads(EModelViewType viewType) const;

        const float getScale() const  { return m_scale; }
        const float getAngleX() const { return m_rotation.x; }
        const float getAngleY() const { return m_rotation.y; }
        const float getAngleZ() const { return m_rotation.z; }

        void move(glm::vec3 pos)  { m_position += pos; }
        void scale(float coef)    { m_scale = coef; }
        void rotateX(float theta) { m_rotation.x = theta; }
        void rotateY(float theta) { m_rotation.y = theta; }
        void rotateZ(float theta) { m_rotation.z = theta; }

    private:
        void processNode(aiNode* node, const aiScene* scene);
        void processMesh(aiMesh* mesh, const aiScene* scene);

        std::list<Texture> loadMaterialTextures(aiMaterial* material, aiTextureType type);
        unsigned textureFromFile(const char* path);

        std::list<Mesh> m_meshes;
        std::list<Mesh> m_subdividedMeshes;
        std::set<const char*> m_loadedTextures;

        ModelConverter meshToModel(const Mesh& mesh);
        void applyCatmullClarkSubdivisionOnce(ModelConverter oldModel, Mesh& newMesh);

        Shader m_shader;

        std::string m_modelDir = "";

        glm::vec3 m_position = glm::vec3(0.0f);
        glm::vec3 m_rotation = glm::vec3(0.0f);
        float m_scale = 1.0f;
    };
}

#endif // CATMULL_CLARK_SUBDIVITION_MODEL_H_
