#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include "Shader.h"
#include "Collider.h"
#include <algorithm>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma = false);

class Model {
public:
    std::vector<Texture> textures_loaded;	
    std::vector<Mesh>    meshes;
    std::string directory;
    bool gammaCorrection;
    AABB localBounds;

    Model(std::string const &path, bool gamma = false) : gammaCorrection(gamma) {
        loadModel(path);
        calculateBounds();
    }
    
    void calculateBounds() {
        if (meshes.empty()) return;
        glm::vec3 minE = meshes[0].localBounds.minExtents;
        glm::vec3 maxE = meshes[0].localBounds.maxExtents;
        for (const auto& m : meshes) {
            minE.x = std::min(minE.x, m.localBounds.minExtents.x);
            minE.y = std::min(minE.y, m.localBounds.minExtents.y);
            minE.z = std::min(minE.z, m.localBounds.minExtents.z);
            maxE.x = std::max(maxE.x, m.localBounds.maxExtents.x);
            maxE.y = std::max(maxE.y, m.localBounds.maxExtents.y);
            maxE.z = std::max(maxE.z, m.localBounds.maxExtents.z);
        }
        localBounds = AABB(minE, maxE);
    }

    void Draw(const Shader &shader) {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }
    
private:
    void loadModel(std::string const &path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }
        directory = path.substr(0, path.find_last_of('/'));
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode *node, const aiScene *scene) {
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            glm::vec3 vector; 
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            if (mesh->HasNormals()) {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            if(mesh->mTextureCoords[0]) {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x; 
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }
        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);        
        }
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    

        // 1. Albedo: albedoMap
        std::vector<Texture> albedoMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "albedoMap", scene);
        textures.insert(textures.end(), albedoMaps.begin(), albedoMaps.end());
        
        // 2. Normal: normalMap
        // Assimp 處理 GLTF/OBJ 時法線可能在 HEIGHT 或 NORMALS 槽位
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "normalMap", scene);
        if(normalMaps.empty()) normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "normalMap", scene);
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        
        // 3. Metallic: metallicMap
        std::vector<Texture> metallicMaps = loadMaterialTextures(material, aiTextureType_METALNESS, "metallicMap", scene);
        textures.insert(textures.end(), metallicMaps.begin(), metallicMaps.end());
        
        // 4. Roughness: roughnessMap
        std::vector<Texture> roughnessMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap", scene);
        if(roughnessMaps.empty()) roughnessMaps = loadMaterialTextures(material, aiTextureType_SHININESS, "roughnessMap", scene);
        textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());
        
        // 5. Ambient Occlusion: aoMap
        std::vector<Texture> aoMaps = loadMaterialTextures(material, aiTextureType_AMBIENT_OCCLUSION, "aoMap", scene);
        if(aoMaps.empty()) aoMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "aoMap", scene);
        textures.insert(textures.end(), aoMaps.begin(), aoMaps.end());

        // 6. Emissive: emissiveMap
        std::vector<Texture> emissiveMaps = loadMaterialTextures(material, aiTextureType_EMISSIVE, "emissiveMap", scene);
        textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
        
        return Mesh(vertices, indices, textures);
    }

    std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, const aiScene *scene) {
        std::vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++) {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                    textures.push_back(textures_loaded[j]);
                    skip = true; 
                    break;
                }
            }
            if(!skip) {
                Texture texture;
                const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());
                if (embeddedTexture) {
                    // 內嵌貼圖處理
                    texture.id = TextureFromMemory(embeddedTexture);
                } else {
                    // 外部路徑處理
                    texture.id = TextureFromFile(str.C_Str(), this->directory);
                }
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture); 
            }
        }
        return textures;
    }

    unsigned int TextureFromMemory(const aiTexture* texture) {
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char *data = nullptr;
        if (texture->mHeight == 0) {
            // 壓縮格式 (JPG/PNG)，mWidth 是位元組長度
            data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture->pcData), texture->mWidth, &width, &height, &nrComponents, 0);
        } else {
            // 未壓縮格式 (RGBA)
            data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture->pcData), texture->mWidth * texture->mHeight * 4, &width, &height, &nrComponents, 0);
        }

        if (data) {
            GLenum format;
            if (nrComponents == 1) format = GL_RED;
            else if (nrComponents == 3) format = GL_RGB;
            else if (nrComponents == 4) format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            stbi_image_free(data);
        }
        return textureID;
    }
};

inline unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma) {
    std::string filename = std::string(path);
    filename = directory + "/" + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
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
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
#endif
