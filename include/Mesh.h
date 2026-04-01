#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Shader.h"
#include <string>
#include <vector>
#include "Collider.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
    int BoneIDs[4];
    float Weights[4];

    Vertex() {
        for (int i = 0; i < 4; i++) {
            BoneIDs[i] = -1;
            Weights[i] = 0.0f;
        }
    }
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture>      textures;
    unsigned int VAO;
    AABB localBounds;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        calculateBounds();
        setupMesh();
    }
    
    void calculateBounds() {
        if (vertices.empty()) return;
        glm::vec3 minE = vertices[0].Position;
        glm::vec3 maxE = vertices[0].Position;
        for (const auto& v : vertices) {
            minE.x = std::min(minE.x, v.Position.x);
            minE.y = std::min(minE.y, v.Position.y);
            minE.z = std::min(minE.z, v.Position.z);
            maxE.x = std::max(maxE.x, v.Position.x);
            maxE.y = std::max(maxE.y, v.Position.y);
            maxE.z = std::max(maxE.z, v.Position.z);
        }
        localBounds = AABB(minE, maxE);
    }

    void Draw(const Shader &shader) {
        for(unsigned int i = 0; i < textures.size(); i++) {
            std::string name = textures[i].type;
            int slot = -1;
            if (name == "albedoMap") slot = 10;
            else if (name == "normalMap") slot = 11;
            else if (name == "metallicMap") slot = 12;
            else if (name == "roughnessMap") slot = 13;
            else if (name == "aoMap") slot = 14;
            else if (name == "emissiveMap") slot = 15;

            if (slot != -1) {
                glActiveTexture(GL_TEXTURE0 + slot);
                glBindTexture(GL_TEXTURE_2D, textures[i].id);
            }
        }
        
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

private:
    unsigned int VBO, EBO;
    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);  

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);	
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);	
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        glEnableVertexAttribArray(2);	
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
        glEnableVertexAttribArray(3);	
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));
        glEnableVertexAttribArray(4);	
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));
        
        // Bone data
        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, BoneIDs));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Weights));

        glBindVertexArray(0);
    }
};
#endif
