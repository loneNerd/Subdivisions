#pragma once
#ifndef CATMULL_CLARK_SUBDIVITION_MODEL_H_
#define CATMULL_CLARK_SUBDIVITION_MODEL_H_

#include <exception>
#include <map>
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
        Vertex() : Position{ glm::vec3(0.0f) }, TexCoord{ glm::vec2(0.0f) } { }

        glm::vec3 Position;
        glm::vec2 TexCoord;
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
        std::vector<glm::uvec4> Quads;
        std::list<Texture>      Textures;
        unsigned VAO;
        unsigned VBO;
        unsigned EBO;
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
        const size_t getQuadsCount(EModelViewType viewType) const;

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

        void applySubdivision(Mesh& oldMesh, Mesh& newMesh);
        int32_t addNewVertex(Mesh& mesh, Vertex& vertex);
        std::string vec3ToString(const glm::vec3& vec3);

        Shader m_shader;

        std::string m_modelDir = "";

        glm::vec3 m_position = glm::vec3(0.0f);
        glm::vec3 m_rotation = glm::vec3(0.0f);
        float m_scale = 1.0f;
    };
}

#endif // CATMULL_CLARK_SUBDIVITION_MODEL_H_
