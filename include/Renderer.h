#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <vector>
#include "Shader.h"
#include "Entity.h"

class Renderer {
public:
    static void renderEntities(const Shader& shader, const std::vector<Entity>& entities, 
                             unsigned int cubeVAO, unsigned int floorVAO, 
                             unsigned int sphereVAO, int sphereCount, 
                             unsigned int icoVAO, int icoCount,
                             unsigned int texDiff, unsigned int texSpec, unsigned int texNorm,
                             unsigned int floorDiff, unsigned int floorNorm,
                             unsigned int waterNorm,
                             unsigned int whiteTex,
                             unsigned int flatNormalTex,
                             bool useNormalMap,
                             glm::vec3 viewPos) {
        
        shader.use();
        shader.setBool("useNormalMap", useNormalMap);
        shader.setVec3("viewPos", viewPos);
        shader.setFloat("time", (float)glfwGetTime());

        // --- 關鍵修正：固定槽位提取光源 ---
        // 初始化為黑，確保沒找到實體時不發光
        shader.setVec3("light1.color", glm::vec3(0.0f));
        shader.setVec3("light2.color", glm::vec3(0.0f));

        for (const auto& e : entities) {
            if (!e.isLight) continue;
            
            // 根據名稱固定綁定槽位
            if (e.name == "Main Sun") {
                shader.setVec3("light1.position", e.position);
                shader.setVec3("light1.color", e.visible ? (e.lightColor * e.lightIntensity) : glm::vec3(0.0f));
            } 
            else if (e.name == "Point Light") {
                shader.setVec3("light2.position", e.position);
                shader.setVec3("light2.color", e.visible ? (e.lightColor * e.lightIntensity) : glm::vec3(0.0f));
            }
        }

        for (const auto& e : entities) {
            if (!e.visible) continue;
            
            glm::mat4 model = e.getModelMatrix();
            shader.setMat4("model", model);

            if (e.isLight) {
                shader.setBool("isLightSource", true);
                shader.setVec3("objectColor", e.lightColor);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                shader.setBool("isLightSource", false);
                continue;
            }
            
            shader.setFloat("roughness", e.roughness);
            shader.setFloat("metallic", e.metallic);
            shader.setFloat("material.ambientStrength", e.ambient);
            shader.setVec3("objectColor", e.color);
            shader.setFloat("reflectivity", e.reflectivity);
            shader.setBool("isWater", e.type == WATER);

            glm::mat4 texMat(1.0f);
            if (e.dynamicTexture && e.type != WATER) {
                texMat = glm::translate(texMat, glm::vec3((float)glfwGetTime() * e.texSpeed, 0.0f, 0.0f));
            }
            shader.setMat4("textureMatrix", texMat);

            if (e.type == FLOOR) {
                glBindVertexArray(floorVAO); 
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, floorDiff);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, whiteTex);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, floorNorm);
                glDrawArrays(GL_TRIANGLES, 0, 18);
            } else if (e.type == CUBE) {
                glBindVertexArray(cubeVAO);
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texDiff);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texSpec);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, texNorm);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            } else if (e.type == SPHERE) {
                glBindVertexArray(sphereVAO); 
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, whiteTex);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, whiteTex);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, flatNormalTex);
                glDrawElements(GL_TRIANGLES, sphereCount, GL_UNSIGNED_INT, 0);
            } else if (e.type == ICOSAHEDRON) {
                glBindVertexArray(icoVAO); 
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, whiteTex);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, whiteTex);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, flatNormalTex);
                glDrawArrays(GL_TRIANGLES, 0, icoCount);
            } else if (e.type == WATER) {
                glBindVertexArray(floorVAO); 
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, floorDiff);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, whiteTex); 
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, waterNorm);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }

    static void renderParticles(const Shader& shader, const std::vector<Entity>& entities, float time, float spread, float size, float particleCount) {
        shader.use();
        shader.setFloat("time", time); shader.setFloat("spread", spread); shader.setFloat("size", size); shader.setFloat("particleCount", particleCount);
        static bool initialized = false; static unsigned int vao, vbo;
        if (!initialized) {
            float point[] = { 0.0f, 0.0f, 0.0f };
            glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
            glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(point), point, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            initialized = true;
        }
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        glBindVertexArray(vao); glPatchParameteri(GL_PATCH_VERTICES, 1);
        for (const auto& e : entities) {
            if (e.type == PARTICLE && e.visible) {
                shader.setVec3("objectColor", e.color);
                shader.setMat4("model", e.getModelMatrix());
                glDrawArrays(GL_PATCHES, 0, 1);
            }
        }
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    }
};

#endif
