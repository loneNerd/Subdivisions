#include "model.h"

#include <algorithm>
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
                                                   aiProcess_FlipUVs);

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

        glDrawElements(GL_TRIANGLES, 4 * static_cast<unsigned>(mesh.Triangles.size()), GL_UNSIGNED_INT, 0);

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

        // texture coordinates
        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            vertex.TexCoord = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        else
            vertex.TexCoord = glm::vec2(-1.0f);

        newMesh.Vertices.push_back(vertex);
    }

    // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for (unsigned i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];

        if (face.mNumIndices != 3)
            throw std::exception(std::string("Model doesn't have correct number of indices (need 3): ").append(std::to_string(face.mNumIndices)).c_str());

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

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, newMesh.Triangles.size() * sizeof(glm::uvec3), newMesh.Triangles.data(), GL_STATIC_DRAW);

    // set the vertex attribute pointers
    // vertex Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // vertex texture coords
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoord));

    glBindVertexArray(0);

    Mesh subdivMesh { };
    applySubdivision(newMesh, subdivMesh);

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

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, subdivMesh.Triangles.size() * sizeof(glm::uvec3), subdivMesh.Triangles.data(), GL_STATIC_DRAW);

    // set the vertex attribute pointers
    // vertex Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // vertex texture coords
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoord));

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

        std::string path = texturePath.C_Str();
        if (path.find_last_of('/') != std::string::npos)
            path = path.substr(path.find_last_of('/') + 1);
        else if (path.find_last_of('\\') != std::string::npos)
            path = path.substr(path.find_last_of('\\') + 1);

        // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        if (m_loadedTextures.find(texturePath.C_Str()) == m_loadedTextures.end())
        {   // if texture hasn't been loaded already, load it
            Texture texture;
            texture.Id = textureFromFile(path.c_str());
            texture.Type = type;
            texture.Path = path.c_str();
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

void Model::applySubdivision(Mesh& oldMesh, Mesh& newMesh)
{
    std::map<std::string, std::set<std::string>> verticesNeighbors;
    std::map <std::string, std::set<std::string>> newVerticesNeighbors;
    std::map<std::string, glm::vec3> stringToVec3;

    for (const glm::uvec3& triangle : oldMesh.Triangles)
    {
        size_t a = newMesh.Vertices.size();
        newMesh.Vertices.emplace_back(oldMesh.Vertices[triangle.x]);
        size_t b = newMesh.Vertices.size();
        newMesh.Vertices.emplace_back(oldMesh.Vertices[triangle.y]);
        size_t c = newMesh.Vertices.size();
        newMesh.Vertices.emplace_back(oldMesh.Vertices[triangle.z]);

        Vertex ab;

        ab.Position = 0.5f * (oldMesh.Vertices[triangle.x].Position + oldMesh.Vertices[triangle.y].Position);
        ab.TexCoord = 0.5f * (oldMesh.Vertices[triangle.x].TexCoord + oldMesh.Vertices[triangle.y].TexCoord);

        int32_t abIndex = addNewVertex(newMesh, ab);

        if (abIndex == -1)
            throw std::exception("Can't add new AB Vertex");

        Vertex bc;

        bc.Position = 0.5f * (oldMesh.Vertices[triangle.y].Position + oldMesh.Vertices[triangle.z].Position);
        bc.TexCoord = 0.5f * (oldMesh.Vertices[triangle.y].TexCoord + oldMesh.Vertices[triangle.z].TexCoord);

        int32_t bcIndex = addNewVertex(newMesh, bc);

        if (bcIndex == -1)
            throw std::exception("Can't add new BC Vertex");

        Vertex ca;

        ca.Position = 0.5f * (oldMesh.Vertices[triangle.z].Position + oldMesh.Vertices[triangle.x].Position);
        ca.TexCoord = 0.5f * (oldMesh.Vertices[triangle.z].TexCoord + oldMesh.Vertices[triangle.x].TexCoord);

        int32_t caIndex = addNewVertex(newMesh, ca);

        if (caIndex == -1)
            throw std::exception("Can't add new CD Vertex");

        newMesh.Triangles.emplace_back(glm::uvec3(abIndex, bcIndex, caIndex));
        newMesh.Triangles.emplace_back(glm::uvec3(caIndex, a, abIndex));
        newMesh.Triangles.emplace_back(glm::uvec3(abIndex, b, bcIndex));
        newMesh.Triangles.emplace_back(glm::uvec3(bcIndex, c, caIndex));

        std::string aPosStr = vec3ToString(newMesh.Vertices[a].Position);
        std::string bPosStr = vec3ToString(newMesh.Vertices[b].Position);
        std::string cPosStr = vec3ToString(newMesh.Vertices[c].Position);

        std::string abPosStr = vec3ToString(ab.Position);
        std::string bcPosStr = vec3ToString(bc.Position);
        std::string caPosStr = vec3ToString(ca.Position);

        stringToVec3[aPosStr] = newMesh.Vertices[a].Position;
        stringToVec3[bPosStr] = newMesh.Vertices[b].Position;
        stringToVec3[cPosStr] = newMesh.Vertices[c].Position;

        stringToVec3[abPosStr] = ab.Position;
        stringToVec3[bcPosStr] = bc.Position;
        stringToVec3[caPosStr] = ca.Position;

        verticesNeighbors[aPosStr].emplace(abPosStr);
        verticesNeighbors[aPosStr].emplace(caPosStr);

        verticesNeighbors[bPosStr].emplace(abPosStr);
        verticesNeighbors[bPosStr].emplace(bcPosStr);

        verticesNeighbors[cPosStr].emplace(bcPosStr);
        verticesNeighbors[cPosStr].emplace(caPosStr);

        glm::vec3 tmpVec = glm::vec3(0.0f);
        std::string tmpStr = "";

        tmpVec = THREE_EIGHT * newMesh.Vertices[a].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[abPosStr].emplace(tmpStr);

        tmpVec = THREE_EIGHT * newMesh.Vertices[b].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[abPosStr].emplace(tmpStr);

        tmpVec = ONE_EIGHT * newMesh.Vertices[c].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[abPosStr].emplace(tmpStr);

        tmpVec = ONE_EIGHT * newMesh.Vertices[a].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[bcPosStr].emplace(tmpStr);

        tmpVec = THREE_EIGHT * newMesh.Vertices[b].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[bcPosStr].emplace(tmpStr);

        tmpVec = THREE_EIGHT * newMesh.Vertices[c].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[bcPosStr].emplace(tmpStr);

        tmpVec = THREE_EIGHT * newMesh.Vertices[a].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[caPosStr].emplace(tmpStr);

        tmpVec = ONE_EIGHT * newMesh.Vertices[b].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[caPosStr].emplace(tmpStr);

        tmpVec = THREE_EIGHT * newMesh.Vertices[c].Position;
        tmpStr = vec3ToString(tmpVec);
        stringToVec3[tmpStr] = tmpVec;
        newVerticesNeighbors[caPosStr].emplace(tmpStr);
    }

    for (Vertex& vertex : newMesh.Vertices)
    {
        std::string posStr = vec3ToString(vertex.Position);

        if (verticesNeighbors.count(posStr))
        {
            size_t n = verticesNeighbors[posStr].size();

            float beta = calculateBeta(n);

            glm::vec3 neighbours = glm::vec3(0.0f);

            for (const std::string& position : verticesNeighbors[posStr])
                neighbours += stringToVec3[position];

            vertex.Position = (1.0f - n * beta) * vertex.Position + neighbours * beta;
        }

        if (newVerticesNeighbors.count(posStr))
        {
            glm::vec3 neighbours = glm::vec3(0.0f);

            for (const std::string& position : newVerticesNeighbors[posStr])
                neighbours += stringToVec3[position];

            vertex.Position = (1.0f - NEW_EDGE_BETA) * vertex.Position + neighbours * NEW_EDGE_BETA;
        }
    }

    int test = 0;
}

int32_t Model::addNewVertex(Mesh& mesh, Vertex& vertex)
{
    int32_t index = -1;
    auto pos = std::find_if(mesh.Vertices.begin(), mesh.Vertices.end(),
        [&vertex](const Vertex& v) { return v.Position == vertex.Position && v.TexCoord == vertex.TexCoord; });

    if (pos == mesh.Vertices.end())
    {
        index = static_cast<int32_t>(mesh.Vertices.size());
        mesh.Vertices.emplace_back(vertex);
    }
    else
        index = static_cast<int32_t>(pos - mesh.Vertices.begin());

    return index;
}

std::string Model::vec3ToString(const glm::vec3& vec3)
{
    return std::to_string(vec3.x).append("|") + std::to_string(vec3.y).append("|") + std::to_string(vec3.z);
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
