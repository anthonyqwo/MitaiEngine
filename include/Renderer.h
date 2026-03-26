#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <vector>
#include "Shader.h"
#include "Entity.h"
#include "Model.h"

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
            
            // --- 核心邏輯修正：互斥渲染 ---
            if (shader.hasTessellation) {
                // 如果是進階 Shader (有 Tessellation)，只渲染 ADV_SPHERE (與粒子，如果有的話)
                if (e.type != ADV_SPHERE && e.type != PARTICLE) continue;
            } else {
                // 如果是普通 Shader，跳過需要細分的物件
                if (e.type == ADV_SPHERE || e.type == PARTICLE) continue;
            }
            
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

            // --- 強制性 PBR 回退機制 (防止狀態殘留) ---
            glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, whiteTex);      // Albedo default
            glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, flatNormalTex); // Normal default
            glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, whiteTex);      // Metallic default
            glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, whiteTex);      // Roughness default
            glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, whiteTex);      // AO default
            glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, flatNormalTex); // 重借用 flatNormal 作為黑色回退，但更專業做法是建一個 blackTex

            // 修正：建立一個黑色貼圖作為自發光回退
            static unsigned int blackTex = 0;
            if (blackTex == 0) {
                glGenTextures(1, &blackTex);
                unsigned char data[] = { 0, 0, 0 };
                glBindTexture(GL_TEXTURE_2D, blackTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            }
            glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, blackTex);

            if (e.type == FLOOR) {
                glBindVertexArray(floorVAO); 
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, floorDiff);
                glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, floorNorm);
                glDrawArrays(GL_TRIANGLES, 0, 18);
            } else if (e.type == CUBE) {
                glBindVertexArray(cubeVAO);
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, texDiff);
                glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, texSpec); // 使用 Spec 作為金屬度回退
                glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, texNorm); // 修正：這裡應該是 normalMap 對應單元 11
                // 重新校正 CUBE 的貼圖單元
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, texDiff);
                glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, texNorm);
                glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, texSpec); 
                glDrawArrays(GL_TRIANGLES, 0, 36);
            } else if (e.type == SPHERE || e.type == ICOSAHEDRON) {
                unsigned int vao = (e.type == SPHERE) ? sphereVAO : icoVAO;
                int count = (e.type == SPHERE) ? sphereCount : icoCount;
                glBindVertexArray(vao); 
                // 使用預設值即可
                if (e.type == SPHERE) glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
                else glDrawArrays(GL_TRIANGLES, 0, count);
            } else if (e.type == WATER) {
                glBindVertexArray(floorVAO); 
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, floorDiff);
                glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, waterNorm);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            } else if (e.type == ADV_SPHERE) {
                glBindVertexArray(icoVAO); 
                glPatchParameteri(GL_PATCH_VERTICES, 3);
                glDrawArrays(GL_PATCHES, 0, icoCount);
            } else if (e.type == MODEL && e.model) {
                e.model->Draw(shader);
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
