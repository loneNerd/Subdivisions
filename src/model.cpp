#include "model.h"

#include <exception>
#include <fstream>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#undef APIENTRY
#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.h"

using namespace CatmullClarkSubdivision;

Model::~Model()
{
    for (Mesh& mesh : m_meshes)
    {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
        
        for (Texture tex : mesh.Textures)
            glDeleteTextures(1, &tex.Id);
    }

    for (Mesh& mesh : m_subdividedMeshes)
    {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);

        for (Texture tex : mesh.Textures)
            glDeleteTextures(1, &tex.Id);
    }

    m_shader.release();
}

void Model::loadModel(const char* path, glm::mat4 projection, glm::mat4 view)
{
    // retrieve the directory path of the filepath
    std::string temp(path);
    if (temp.find_last_of('/') != std::string::npos)
        m_modelDir = temp.substr(0, temp.find_last_of('/'));
    else if (temp.find_last_of('\\') != std::string::npos)
        m_modelDir = temp.substr(0, temp.find_last_of('\\'));

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(false);

    // read file via ASSIMP
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_JoinIdenticalVertices |
                                                   aiProcess_Triangulate           |
                                                   aiProcess_GenSmoothNormals      |
                                                   aiProcess_FlipUVs               |
                                                   aiProcess_CalcTangentSpace);

    // check for errors
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        throw std::exception(std::string("ASSIMP: ").append(importer.GetErrorString()).c_str());

    // process ASSIMP's root node recursively
    processNode(scene->mRootNode, scene);

    m_shader.loadShader(getFileFullPath("shaders\\vertex.vs").c_str(),
                        getFileFullPath("shaders\\fragment.fs").c_str());

    m_shader.use();
    m_shader.setMat4("view", view);
    m_shader.setMat4("projection", projection);
}

void Model::draw(EModelViewType viewType)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::rotate(model, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(1.0f) * m_scale);

    m_shader.use();

    m_shader.setMat4("model", model);

    std::list<Mesh> meshes = m_meshes;

    if (viewType == EModelViewType::ESubdiveded)
        meshes = m_subdividedMeshes;

    // draw meshes
    for (Mesh& mesh : meshes)
    {
        // bind appropriate textures
        unsigned diffuseNr = 1;
        unsigned specularNr = 1;
        unsigned normalNr = 1;
        unsigned heightNr = 1;

        auto it = mesh.Textures.cbegin();

        for (unsigned i = 0; i < mesh.Textures.size(); ++i, ++it)
        {
            glActiveTexture(GL_TEXTURE0 + i); // active proper texture unit before binding
            // retrieve texture number (the N in diffuse_textureN)

            if (it->Type == aiTextureType_DIFFUSE)
            {
                const char* tmp = std::string("diffuse_").append(std::to_string(diffuseNr)).c_str();
                m_shader.setInt(tmp, i);
                ++diffuseNr;
            }
            else if (it->Type == aiTextureType_SPECULAR)
            {
                const char* tmp = std::string("specular_").append(std::to_string(specularNr)).c_str();
                m_shader.setInt(tmp, i);
                ++specularNr;
            }
            else if (it->Type == aiTextureType_HEIGHT)
            {
                const char* tmp = std::string("normal_").append(std::to_string(normalNr)).c_str();
                m_shader.setInt(tmp, i);
                ++normalNr;
            }
            else if (it->Type == aiTextureType_AMBIENT)
            {
                const char* tmp = std::string("height_").append(std::to_string(heightNr)).c_str();
                m_shader.setInt(tmp, i);
                ++heightNr;
            }

            // now set the sampler to the correct texture unit
            // and finally bind the texture
            glBindTexture(GL_TEXTURE_2D, it->Id);
        }

        // draw mesh
        glBindVertexArray(mesh.VAO);

        if (mesh.IsQuads)
            glDrawElements(GL_QUADS, 4 * static_cast<unsigned>(mesh.Quads.size()), GL_UNSIGNED_INT, 0);
        else
            glDrawElements(GL_TRIANGLES, 3 * static_cast<unsigned>(mesh.Triangles.size()), GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);

        // always good practice to set everything back to defaults once configured.
        glActiveTexture(GL_TEXTURE0);
    }
}

// processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
void Model::processNode(aiNode* node, const aiScene* scene)
{
    // process each mesh located at the current node
    for (unsigned i = 0; i < node->mNumMeshes; ++i)
    {
        // the node object only contains indices to index the actual objects in the scene. 
        // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        processMesh(mesh, scene);
    }
    // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
    for (unsigned i = 0; i < node->mNumChildren; ++i)
        processNode(node->mChildren[i], scene);
}

void Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
    Mesh newMesh { };

    // walk through each of the mesh's vertices
    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex vertex { };
        vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        // normals
        if (mesh->HasNormals())
            vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

        // texture coordinates
        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            vertex.TextureCoordinate = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        else
            vertex.TextureCoordinate = glm::vec2(0.0f, 0.0f);

        newMesh.Vertices.push_back(vertex);
    }

    newMesh.IsQuads = false;

    // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for (unsigned i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];

        // retrieve all indices of the face and store them in the indices vector
        newMesh.Triangles.emplace_back(glm::uvec3(face.mIndices[0], face.mIndices[1], face.mIndices[2]));
    }

    // process materials
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

    std::list<Texture> textures;
    // 1. diffuse maps
    std::list<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE);
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::list<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR);
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::list<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT);
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::list<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT);
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

    newMesh.Textures = textures;

    // create buffers/arrays
    glGenVertexArrays(1, &newMesh.VAO);
    glGenBuffers(1, &newMesh.VBO);
    glGenBuffers(1, &newMesh.EBO);

    glBindVertexArray(newMesh.VAO);
    // load data into vertex buffers
    glBindBuffer(GL_ARRAY_BUFFER, newMesh.VBO);

    // A great thing about structs is that their memory layout is sequential for all its items.
    // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
    // again translates to 3/2 floats which translates to a byte array.
    glBufferData(GL_ARRAY_BUFFER, newMesh.Vertices.size() * sizeof(Vertex), newMesh.Vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh.EBO);

    if (newMesh.IsQuads)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, newMesh.Quads.size() * sizeof(glm::uvec4), newMesh.Quads.data(), GL_STATIC_DRAW);
    else
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, newMesh.Triangles.size() * sizeof(glm::uvec3), newMesh.Triangles.data(), GL_STATIC_DRAW);

    // set the vertex attribute pointers
    // vertex Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    // vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TextureCoordinate));
/*
    // vertex tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

    // vertex bitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));

    // ids
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, BoneIds));

    // weights
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Weights));
    */
    glBindVertexArray(0);

    Mesh subdivMesh = newMesh;
    subdivMesh.Vertices.clear();
    subdivMesh.Triangles.clear();
    subdivMesh.Quads.clear();
    subdivMesh.IsQuads = true;

    applyCatmullClarkSubdivisionOnce(meshToModel(newMesh), subdivMesh);

    // create buffers/arrays
    glGenVertexArrays(1, &subdivMesh.VAO);
    glGenBuffers(1, &subdivMesh.VBO);
    glGenBuffers(1, &subdivMesh.EBO);

    glBindVertexArray(subdivMesh.VAO);
    // load data into vertex buffers
    glBindBuffer(GL_ARRAY_BUFFER, subdivMesh.VBO);

    // A great thing about structs is that their memory layout is sequential for all its items.
    // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
    // again translates to 3/2 floats which translates to a byte array.
    glBufferData(GL_ARRAY_BUFFER, subdivMesh.Vertices.size() * sizeof(Vertex), subdivMesh.Vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subdivMesh.EBO);

    if (subdivMesh.IsQuads)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, subdivMesh.Quads.size() * sizeof(glm::uvec4), subdivMesh.Quads.data(), GL_STATIC_DRAW);
    else
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, subdivMesh.Triangles.size() * sizeof(glm::uvec3), subdivMesh.Triangles.data(), GL_STATIC_DRAW);

    // set the vertex attribute pointers
    // vertex Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    // vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TextureCoordinate));

    glBindVertexArray(0);

    m_subdividedMeshes.emplace_back(subdivMesh);

    m_meshes.emplace_back(newMesh);
}

// Checks all material textures of a given type and loads the textures if they're not loaded yet.
// the required info is returned as a Texture struct.
std::list<Texture> Model::loadMaterialTextures(aiMaterial* material, aiTextureType type)
{
    std::list<Texture> textures;
    for (unsigned int i = 0; i < material->GetTextureCount(type); ++i)
    {
        aiString texturePath;
        material->GetTexture(type, i, &texturePath);

        // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        if (m_loadedTextures.find(texturePath.C_Str()) == m_loadedTextures.end())
        {   // if texture hasn't been loaded already, load it
            Texture texture;
            texture.Id = textureFromFile(texturePath.C_Str());
            texture.Type = type;
            texture.Path = texturePath.C_Str();
            textures.push_back(texture);
            m_loadedTextures.insert(texturePath.C_Str()); // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
        }
    }
    return textures;
}

unsigned Model::textureFromFile(const char* path)
{
    std::string filename = m_modelDir + std::string("\\").append(path);

    unsigned textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

ModelConverter Model::meshToModel(const Mesh& mesh)
{
    ModelConverter model;

    size_t count = mesh.IsQuads ? mesh.Quads.size() : mesh.Triangles.size();

    for (size_t i = 0; i < count; ++i)
    {
        int vertexCount = mesh.IsQuads ? 4 : 3;
        int vertexIndices[4] = { -1, -1, -1, -1 };

        if (mesh.IsQuads)
        {
            vertexIndices[0] = model.getVertexIndex(mesh.Vertices[mesh.Quads[i].x].Position);
            vertexIndices[1] = model.getVertexIndex(mesh.Vertices[mesh.Quads[i].y].Position);
            vertexIndices[2] = model.getVertexIndex(mesh.Vertices[mesh.Quads[i].z].Position);
            vertexIndices[3] = model.getVertexIndex(mesh.Vertices[mesh.Quads[i].w].Position);
        }
        else
        {
            vertexIndices[0] = model.getVertexIndex(mesh.Vertices[mesh.Triangles[i].x].Position);
            vertexIndices[1] = model.getVertexIndex(mesh.Vertices[mesh.Triangles[i].y].Position);
            vertexIndices[2] = model.getVertexIndex(mesh.Vertices[mesh.Triangles[i].z].Position);
        }

        int edgeCount = vertexCount;
        int edgeIndices[4] = { -1, -1, -1, -1 };

        for (int i = 0; i < edgeCount; ++i)
        {
            int startVertex = vertexIndices[i];
            int endVertex = vertexIndices[(i + 1) % vertexCount];
            int edgeIndex = model.getEdgeIndex({ startVertex, endVertex });

            edgeIndices[i] = edgeIndex;

            if (startVertex < endVertex)
            {
                model.Vertices[startVertex].Edges.push_back(edgeIndex);
                model.Vertices[endVertex].Edges.push_back(edgeIndex);
            }
        }

        FaceRecord newFaceRecord;
        newFaceRecord.IsQuad = mesh.IsQuads;

        for (int i = 0; i < vertexCount; ++i)
            newFaceRecord.Vertices[i] = vertexIndices[i];

        for (int i = 0; i < edgeCount; ++i)
            newFaceRecord.Edges[i] = edgeIndices[i];

        int faceIndex = static_cast<int>(model.Faces.size());
        model.Faces.push_back(newFaceRecord);

        for (int i = 0; i < vertexCount; ++i)
        {
            auto& vertexRecord = model.Vertices[vertexIndices[i]];
            vertexRecord.Faces.push_back(faceIndex);
        }

        for (int i = 0; i < edgeCount; ++i)
        {
            EdgeRecord& edgeRecord = model.Edges[edgeIndices[i]];
            if (edgeRecord.FaceCount >= 2)
                throw std::exception("topology error: edge.faceCount > 2");

            edgeRecord.Faces[edgeRecord.FaceCount++] = faceIndex;
        }
    }

    return model;
}

void Model::applyCatmullClarkSubdivisionOnce(ModelConverter oldModel, Mesh& newMesh)
{
    std::vector<glm::vec3> facePoints(oldModel.Faces.size());
    for (size_t i = 0; i < facePoints.size(); ++i)
    {
        FaceRecord& f = oldModel.Faces[i];
        int vertexCount = f.IsQuad ? 4 : 3;

        glm::vec3 vertexSum = glm::vec3(0.0f);

        for (int j = 0; j < vertexCount; ++j)
            vertexSum += oldModel.Vertices[f.Vertices[j]].Position;

        facePoints[i] = vertexSum / static_cast<float>(vertexCount);
    }

    std::vector<glm::vec3> edgePoints(oldModel.Edges.size());
    for (size_t i = 0; i < oldModel.Edges.size(); ++i)
    {
        EdgeRecord& e = oldModel.Edges[i];
        if (e.FaceCount != 2)
        {
            edgePoints[i] = 0.5f * (
                oldModel.Vertices[e.LowVertex].Position +
                oldModel.Vertices[e.HighVertex].Position);
        }
        else
        {
            edgePoints[i] = 0.25f * (
                oldModel.Vertices[e.LowVertex].Position +
                oldModel.Vertices[e.HighVertex].Position +
                facePoints[e.Faces[0]] + facePoints[e.Faces[1]]);
        }
    }

    std::vector<glm::vec3> oldPositions;
    oldPositions.reserve(oldModel.Vertices.size());

    for (auto& v : oldModel.Vertices)
        oldPositions.push_back(v.Position);

    for (size_t i = 0; i < oldModel.Vertices.size(); ++i)
    {
        VertexRecord& v = oldModel.Vertices[i];

        int n = static_cast<int>(v.Faces.size());
        float m1 = static_cast<float>(n - 3) / n;
        float m2 = 1.0f / n;
        float m3 = 2.0f / n;

        glm::vec3 avgFacePosition = glm::vec3(0.0f);
        for (auto fi : v.Faces)
            avgFacePosition += facePoints[fi];

        avgFacePosition /= static_cast<float>(n);

        glm::vec3 avgEdgeMid = glm::vec3(0.0f);

        for (auto edgeIndex : v.Edges)
        {
            EdgeRecord& edge = oldModel.Edges[edgeIndex];
            avgEdgeMid += 0.5f * (oldPositions[edge.LowVertex] + oldPositions[edge.HighVertex]);
        }

        avgEdgeMid /= static_cast<float>(v.Edges.size());

        glm::vec3 newPosition = m1 * oldPositions[i] + m2 * avgFacePosition + m3 * avgEdgeMid;
        oldModel.moveVertex(static_cast<int>(i), newPosition);
    }

    for (size_t fi = 0; fi < oldModel.Faces.size(); ++fi)
    {
        FaceRecord& face = oldModel.Faces[fi];

        if (face.IsQuad)
        {
            glm::vec3 va = oldModel.Vertices[face.Vertices[0]].Position;
            glm::vec3 vb = oldModel.Vertices[face.Vertices[1]].Position;
            glm::vec3 vc = oldModel.Vertices[face.Vertices[2]].Position;
            glm::vec3 vd = oldModel.Vertices[face.Vertices[3]].Position;

            glm::vec3 eab = edgePoints[oldModel.getEdgeIndex({ face.Vertices[0], face.Vertices[1] })];
            glm::vec3 ebc = edgePoints[oldModel.getEdgeIndex({ face.Vertices[1], face.Vertices[2] })];
            glm::vec3 ecd = edgePoints[oldModel.getEdgeIndex({ face.Vertices[2], face.Vertices[3] })];
            glm::vec3 eda = edgePoints[oldModel.getEdgeIndex({ face.Vertices[3], face.Vertices[0] })];

            glm::vec3 fabcd = facePoints[fi];

            unsigned aIndex = static_cast<unsigned>(newMesh.Vertices.size());
            unsigned bIndex = aIndex + 1;
            unsigned cIndex = aIndex + 2;
            unsigned dIndex = aIndex + 3;

            unsigned eabIndex = aIndex + 4;
            unsigned ebcIndex = aIndex + 5;
            unsigned ecdIndex = aIndex + 6;
            unsigned edaIndex = aIndex + 7;

            unsigned fabcdIndex = aIndex + 8;

            newMesh.Vertices.push_back({ va });
            newMesh.Vertices.push_back({ vb });
            newMesh.Vertices.push_back({ vc });
            newMesh.Vertices.push_back({ vd });
            newMesh.Vertices.push_back({ eab });
            newMesh.Vertices.push_back({ ebc });
            newMesh.Vertices.push_back({ ecd });
            newMesh.Vertices.push_back({ eda });
            newMesh.Vertices.push_back({ fabcd });

            newMesh.Quads.push_back(glm::uvec4(edaIndex, aIndex, eabIndex, fabcdIndex));
            newMesh.Quads.push_back(glm::uvec4(eabIndex, bIndex, ebcIndex, fabcdIndex));
            newMesh.Quads.push_back(glm::uvec4(ebcIndex, cIndex, ecdIndex, fabcdIndex));
            newMesh.Quads.push_back(glm::uvec4(ecdIndex, dIndex, edaIndex, fabcdIndex));
        }
        else
        {
            glm::vec3 va = oldModel.Vertices[face.Vertices[0]].Position;
            glm::vec3 vb = oldModel.Vertices[face.Vertices[1]].Position;
            glm::vec3 vc = oldModel.Vertices[face.Vertices[2]].Position;

            glm::vec3 eab = edgePoints[oldModel.getEdgeIndex({ face.Vertices[0], face.Vertices[1] })];
            glm::vec3 ebc = edgePoints[oldModel.getEdgeIndex({ face.Vertices[1], face.Vertices[2] })];
            glm::vec3 eca = edgePoints[oldModel.getEdgeIndex({ face.Vertices[2], face.Vertices[0] })];

            glm::vec3 fabc = facePoints[fi];

            unsigned aIndex = static_cast<unsigned>(newMesh.Vertices.size());
            unsigned bIndex = aIndex + 1;
            unsigned cIndex = aIndex + 2;

            unsigned eabIndex = aIndex + 3;
            unsigned ebcIndex = aIndex + 4;
            unsigned ecaIndex = aIndex + 5;

            unsigned fabcIndex = aIndex + 6;

            newMesh.Vertices.push_back({ va });
            newMesh.Vertices.push_back({ vb });
            newMesh.Vertices.push_back({ vc });
            newMesh.Vertices.push_back({ eab });
            newMesh.Vertices.push_back({ ebc });
            newMesh.Vertices.push_back({ eca });
            newMesh.Vertices.push_back({ fabc });

            newMesh.Quads.push_back(glm::uvec4(ecaIndex, aIndex, eabIndex, fabcIndex));
            newMesh.Quads.push_back(glm::uvec4(eabIndex, bIndex, ebcIndex, fabcIndex));
            newMesh.Quads.push_back(glm::uvec4(ebcIndex, cIndex, ecaIndex, fabcIndex));
        }
    }
}

const size_t Model::getVerticesCount(EModelViewType viewType) const
{
    size_t total = 0;

    if (viewType == EModelViewType::EOriginal)
    {
        for (const Mesh& mesh : m_meshes)
            total += mesh.Vertices.size();

        return total;
    }

    for (const Mesh& mesh : m_subdividedMeshes)
        total += mesh.Vertices.size();

    return total;
}

const size_t Model::getTrianglesCount(EModelViewType viewType) const
{
    size_t total = 0;

    if (viewType == EModelViewType::EOriginal)
    {
        for (const Mesh& mesh : m_meshes)
            total += mesh.Triangles.size();

        return total;
    }

    for (const Mesh& mesh : m_subdividedMeshes)
        total += mesh.Triangles.size();

    return total;
}

const size_t Model::getQuadsCount(EModelViewType viewType) const
{
    size_t total = 0;

    if (viewType == EModelViewType::EOriginal)
    {
        for (const Mesh& mesh : m_meshes)
            total += mesh.Quads.size();

        return total;
    }

    for (const Mesh& mesh : m_subdividedMeshes)
        total += mesh.Quads.size();

    return total;
}

const bool Model::isQuads(EModelViewType viewType) const
{
    bool total = false;

    if (viewType == EModelViewType::EOriginal)
    {
        for (const Mesh& mesh : m_meshes)
            total = mesh.IsQuads;

        return total;
    }

    for (const Mesh& mesh : m_subdividedMeshes)
        total = mesh.IsQuads;

    return total;
}

void ModelConverter::moveVertex(int vertexIndex, const glm::vec3& newPosition)
{
    std::string oldPosition = std::to_string(Vertices[vertexIndex].Position.x) + "|" +
        std::to_string(Vertices[vertexIndex].Position.y) + "|" +
        std::to_string(Vertices[vertexIndex].Position.z);

    std::string newPos = std::to_string(newPosition.x) + "|" +
        std::to_string(newPosition.y) + "|" +
        std::to_string(newPosition.z);

    m_positionToVertex.erase(oldPosition);

    if (m_positionToVertex.find(newPos) != m_positionToVertex.end())
    {
        throw std::exception(
            "topology error in moving vertex: same position for different vertices");
    }

    m_positionToVertex[newPos] = vertexIndex;
    Vertices[vertexIndex].Position = newPosition;
}

int ModelConverter::getVertexIndex(const glm::vec3& position)
{
    std::string pos = std::to_string(position.x) + "|" +
        std::to_string(position.y) + "|" +
        std::to_string(position.z);

    auto it = m_positionToVertex.find(pos);

    if (it != m_positionToVertex.end())
        return it->second;

    int ret = static_cast<int>(Vertices.size());

    VertexRecord newVertexRecord;
    newVertexRecord.Position = position;
    Vertices.push_back(newVertexRecord);
    m_positionToVertex[pos] = ret;

    return ret;
}

int ModelConverter::getEdgeIndex(const glm::uvec2& vertexPair)
{
    glm::uvec2 sortedVertexPair = vertexPair.x < vertexPair.y ? vertexPair : glm::uvec2(vertexPair.y, vertexPair.x);
    std::string sortedVertexPairStr = vertexPair.x < vertexPair.y ?
        std::to_string(vertexPair.x) + "|" +
        std::to_string(vertexPair.y) :
        std::to_string(vertexPair.y) + "|" +
        std::to_string(vertexPair.x);

    auto it = m_vertexPairToEdge.find(sortedVertexPairStr);
    if (it != m_vertexPairToEdge.end())
        return it->second;

    int ret = static_cast<int>(Edges.size());

    EdgeRecord newEdgeRecord;
    newEdgeRecord.LowVertex = sortedVertexPair.x;
    newEdgeRecord.HighVertex = sortedVertexPair.y;
    Edges.push_back(newEdgeRecord);
    m_vertexPairToEdge[sortedVertexPairStr] = ret;

    return ret;
}
