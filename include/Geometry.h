#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

class Geometry {
public:
    static void setupSphere(unsigned int& vao, unsigned int& vbo, int& count) {
        std::vector<float> data;
        const int X_SEGMENTS = 64, Y_SEGMENTS = 64;
        for (int y = 0; y <= Y_SEGMENTS; ++y) {
            for (int x = 0; x <= X_SEGMENTS; ++x) {
                float xSeg = (float)x / (float)X_SEGMENTS;
                float ySeg = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSeg * 2.0f * glm::pi<float>()) * std::sin(ySeg * glm::pi<float>());
                float yPos = std::cos(ySeg * glm::pi<float>());
                float zPos = std::sin(xSeg * 2.0f * glm::pi<float>()) * std::sin(ySeg * glm::pi<float>());
                
                // Position
                data.push_back(xPos); data.push_back(yPos); data.push_back(zPos);
                // Normal
                data.push_back(xPos); data.push_back(yPos); data.push_back(zPos);
                // UV
                data.push_back(xSeg); data.push_back(ySeg);
                // Tangent (球面切線計算)
                float tx = -std::sin(xSeg * 2.0f * glm::pi<float>());
                float ty = 0.0f;
                float tz = std::cos(xSeg * 2.0f * glm::pi<float>());
                data.push_back(tx); data.push_back(ty); data.push_back(tz);
            }
        }
        std::vector<unsigned int> indices;
        for (int y = 0; y < Y_SEGMENTS; ++y) {
            for (int x = 0; x < X_SEGMENTS; ++x) {
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + (x + 1));
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + (x + 1));
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + (x + 1));
            }
        }
        unsigned int ebo;
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        // Stride = 11 (3 Pos, 3 Norm, 2 UV, 3 Tangent)
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
        count = indices.size();
    }

    static void setupIcosahedron(unsigned int& vao, unsigned int& vbo, int& count) {
        float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
        std::vector<glm::vec3> vertices = {
            glm::normalize(glm::vec3(-1, t, 0)), glm::normalize(glm::vec3(1, t, 0)), 
            glm::normalize(glm::vec3(-1,-t, 0)), glm::normalize(glm::vec3(1,-t, 0)),
            glm::normalize(glm::vec3( 0,-1, t)), glm::normalize(glm::vec3( 0, 1, t)),  
            glm::normalize(glm::vec3( 0,-1,-t)), glm::normalize(glm::vec3( 0, 1,-t)),
            glm::normalize(glm::vec3( t, 0,-1)), glm::normalize(glm::vec3( t, 0, 1)), 
            glm::normalize(glm::vec3(-t, 0,-1)), glm::normalize(glm::vec3(-t, 0, 1))
        };
        std::vector<unsigned int> indices = {
            0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
            1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
            3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
            4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
        };
        std::vector<float> data;
        for(size_t i = 0; i < indices.size(); i += 3) {
            glm::vec3 v1 = vertices[indices[i]];
            glm::vec3 v2 = vertices[indices[i+1]];
            glm::vec3 v3 = vertices[indices[i+2]];
            
            // 計算面法線 (實現有稜有角的硬邊效果)
            glm::vec3 normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
            
            // 簡單切線計算 (確保 PBR 光照方向正確)
            glm::vec3 edge = v2 - v1;
            glm::vec3 tangent = glm::normalize(edge - glm::dot(edge, normal) * normal);

            glm::vec3 faceVertices[3] = {v1, v2, v3};
            glm::vec2 faceUVs[3] = {glm::vec2(0,0), glm::vec2(1,0), glm::vec2(0.5,1)};

            for(int j=0; j<3; j++) {
                data.push_back(faceVertices[j].x); data.push_back(faceVertices[j].y); data.push_back(faceVertices[j].z);
                data.push_back(normal.x); data.push_back(normal.y); data.push_back(normal.z);
                data.push_back(faceUVs[j].x); data.push_back(faceUVs[j].y);
                data.push_back(tangent.x); data.push_back(tangent.y); data.push_back(tangent.z);
            }
        }
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
        count = data.size() / 11;
    }
};

#endif
