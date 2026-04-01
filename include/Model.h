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

#include <glm/gtx/quaternion.hpp>

unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma = false);

struct BoneInfo {
    int id;
    glm::mat4 offset;
};

class Model {
public:
    std::vector<Texture> textures_loaded;	
    std::vector<Mesh>    meshes;
    std::string directory;
    bool gammaCorrection;
    AABB localBounds;
    
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;
    
    struct NodeData {
        glm::mat4 transformation;
        std::string name;
        int childrenCount;
        std::vector<NodeData> children;
    };
    NodeData m_RootNode;

    Model(std::string const &path, bool gamma = false) : gammaCorrection(gamma) {
        loadModel(path);
        calculateBounds();
    }
    
    auto& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount() { return m_BoneCounter; }
    const aiScene* GetScene() const { return m_Scene; }
    
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

    void UpdateBoneMatrices(const aiNode* node, glm::mat4 parentTransform, std::vector<glm::mat4>& finalMatrices) {
        std::string nodeName = node->mName.data;
        glm::mat4 nodeTransform = ConvertMatrixToGLMFormat(node->mTransformation);

        glm::mat4 globalTransformation = parentTransform * nodeTransform;

        if (m_BoneInfoMap.find(nodeName) != m_BoneInfoMap.end()) {
            int index = m_BoneInfoMap[nodeName].id;
            glm::mat4 offset = m_BoneInfoMap[nodeName].offset;
            finalMatrices[index] = globalTransformation * offset;
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
            UpdateBoneMatrices(node->mChildren[i], globalTransformation, finalMatrices);
    }

    void Draw(const Shader &shader) {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }
    
private:
    const aiScene* m_Scene = nullptr;
    Assimp::Importer m_Importer;

    void loadModel(std::string const &path) {
        unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_PopulateArmatureData | aiProcess_GlobalScale;
        m_Scene = m_Importer.ReadFile(path, flags);
        if(!m_Scene || m_Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_Scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << m_Importer.GetErrorString() << std::endl;
            return;
        }
        size_t lastSep = path.find_last_of("/\\");
        directory = (lastSep == std::string::npos) ? "." : path.substr(0, lastSep);
        processNode(m_Scene->mRootNode, m_Scene);
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

    void SetVertexBoneData(Vertex& vertex, int boneID, float weight) {
        for (int i = 0; i < 4; ++i) {
            if (vertex.BoneIDs[i] < 0) {
                vertex.Weights[i] = weight;
                vertex.BoneIDs[i] = boneID;
                break;
            }
        }
    }

    static inline glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from) {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene) {
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            int boneID = -1;
            std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
            if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end()) {
                BoneInfo newBoneInfo;
                newBoneInfo.id = m_BoneCounter;
                newBoneInfo.offset = ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
                m_BoneInfoMap[boneName] = newBoneInfo;
                boneID = m_BoneCounter;
                m_BoneCounter++;
            } else {
                boneID = m_BoneInfoMap[boneName].id;
            }
            auto weights = mesh->mBones[boneIndex]->mWeights;
            int numWeights = mesh->mBones[boneIndex]->mNumWeights;

            for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex) {
                int vertexId = weights[weightIndex].mVertexId;
                float weight = weights[weightIndex].mWeight;
                SetVertexBoneData(vertices[vertexId], boneID, weight);
            }
        }
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            vertex.Position.x = mesh->mVertices[i].x;
            vertex.Position.y = mesh->mVertices[i].y;
            vertex.Position.z = mesh->mVertices[i].z;
            if (mesh->HasNormals()) {
                vertex.Normal.x = mesh->mNormals[i].x;
                vertex.Normal.y = mesh->mNormals[i].y;
                vertex.Normal.z = mesh->mNormals[i].z;
            }
            if(mesh->mTextureCoords[0]) {
                vertex.TexCoords.x = mesh->mTextureCoords[0][i].x; 
                vertex.TexCoords.y = mesh->mTextureCoords[0][i].y;
                vertex.Tangent.x = mesh->mTangents[i].x;
                vertex.Tangent.y = mesh->mTangents[i].y;
                vertex.Tangent.z = mesh->mTangents[i].z;
                vertex.Bitangent.x = mesh->mBitangents[i].x;
                vertex.Bitangent.y = mesh->mBitangents[i].y;
                vertex.Bitangent.z = mesh->mBitangents[i].z;
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }

        ExtractBoneWeightForVertices(vertices, mesh, scene);

        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);        
        }
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    

        // 1. Albedo: albedoMap
        std::vector<Texture> albedoMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "albedoMap", scene);
        if(albedoMaps.empty()) albedoMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "albedoMap", scene);
        if(albedoMaps.empty()) albedoMaps = loadMaterialTextures(material, aiTextureType_UNKNOWN, "albedoMap", scene);
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
        if(aoMaps.empty()) aoMaps = loadMaterialTextures(material, aiTextureType_LIGHTMAP, "aoMap", scene);
        if(aoMaps.empty()) aoMaps = loadMaterialTextures(material, aiTextureType_UNKNOWN, "aoMap", scene);
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
    // Replace backslashes with forward slashes for cross-platform compatibility
    std::replace(filename.begin(), filename.end(), '\\', '/');
    
    std::string fullPath = directory + "/" + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);
    
    // If failed, try loading from the directory directly, and try different extensions (common in MMD/PMX)
    if (!data) {
        size_t lastSlash = filename.find_last_of('/');
        std::string leafName = (lastSlash != std::string::npos) ? filename.substr(lastSlash + 1) : filename;
        
        fullPath = directory + "/" + leafName;
        data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);
        
        // Extension fallback
        if (!data) {
            size_t dotPos = leafName.find_last_of('.');
            if (dotPos != std::string::npos) {
                std::string baseName = leafName.substr(0, dotPos);
                const char* exts[] = {".png", ".bmp", ".jpg", ".tga", ".jpeg"};
                for (const char* ext : exts) {
                    fullPath = directory + "/" + baseName + ext;
                    data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);
                    if (data) break;
                }
            }
        }
    }

    if (data) {
        GLenum format = GL_RGB;
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
    } else {
        std::cout << "ERROR::TEXTURE: Failed to load at path: " << fullPath << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
#endif
