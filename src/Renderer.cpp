#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <GLFW/glfw3.h>
#include "Geometry.h"
#include "Model.h"

Renderer::Renderer(unsigned int scrWidth, unsigned int scrHeight) 
    : SCR_WIDTH(scrWidth), SCR_HEIGHT(scrHeight) {
    setupGBuffer();
    setupShadows();
    setupGeometry();
}

Renderer::~Renderer() { }

void Renderer::setupGBuffer() {
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    glGenTextures(1, &gPosition); glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    glGenTextures(1, &gNormal); glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    glGenTextures(1, &gAlbedoSpec); glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

    glGenTextures(1, &gPBR); glBindTexture(GL_TEXTURE_2D, gPBR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gPBR, 0);

    unsigned int attachments[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(4, attachments);

    glGenRenderbuffers(1, &rboDepth); glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::setupShadows() {
    glGenFramebuffers(1, &depthFBO); glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float bCol[] = { 1,1,1,1 }; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bCol);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &pointShadowFBO); glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    for(int i=0; i<6; i++) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO); glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::setupGeometry() {
    float cubeV[] = { -0.5f,-0.5f,-0.5f,0,0,-1,0,0,1,0,0, 0.5f,-0.5f,-0.5f,0,0,-1,1,0,1,0,0, 0.5f,0.5f,-0.5f,0,0,-1,1,1,1,0,0, 0.5f,0.5f,-0.5f,0,0,-1,1,1,1,0,0, -0.5f,0.5f,-0.5f,0,0,-1,0,1,1,0,0, -0.5f,-0.5f,-0.5f,0,0,-1,0,0,1,0,0, -0.5f,-0.5f,0.5f,0,0,1,0,0,1,0,0, 0.5f,-0.5f,0.5f,0,0,1,1,0,1,0,0, 0.5f,0.5f,0.5f,0,0,1,1,1,1,0,0, 0.5f,0.5f,0.5f,0,0,1,1,1,1,0,0, -0.5f,0.5f,0.5f,0,0,1,0,1,1,0,0, -0.5f,-0.5f,0.5f,0,0,1,0,0,1,0,0, -0.5f,0.5f,0.5f,-1,0,0,1,0,0,1,0, -0.5f,0.5f,-0.5f,-1,0,0,1,1,0,1,0, -0.5f,-0.5f,-0.5f,-1,0,0,0,1,0,1,0, -0.5f,-0.5f,-0.5f,-1,0,0,0,1,0,1,0, -0.5f,-0.5f,0.5f,-1,0,0,0,0,0,1,0, -0.5f,0.5f,0.5f,-1,0,0,1,0,0,1,0, 0.5f,0.5f,0.5f,1,0,0,1,0,0,-1,0, 0.5f,0.5f,-0.5f,1,0,0,1,1,0,-1,0, 0.5f,-0.5f,-0.5f,1,0,0,0,1,0,-1,0, 0.5f,-0.5f,-0.5f,1,0,0,0,1,0,-1,0, 0.5f,-0.5f,0.5f,1,0,0,0,0,0,-1,0, 0.5f,0.5f,0.5f,1,0,0,1,0,0,-1,0, -0.5f,-0.5f,-0.5f,0,-1,0,0,1,1,0,0, 0.5f,-0.5f,-0.5f,0,-1,0,1,1,1,0,0, 0.5f,-0.5f,0.5f,0,-1,0,1,0,1,0,0, 0.5f,-0.5f,0.5f,0,-1,0,1,0,1,0,0, -0.5f,-0.5f,0.5f,0,-1,0,0,0,1,0,0, -0.5f,-0.5f,-0.5f,0,-1,0,0,1,1,0,0, -0.5f,0.5f,-0.5f,0,1,0,0,1,1,0,0, 0.5f,0.5f,-0.5f,0,1,0,1,1,1,0,0, 0.5f,0.5f,0.5f,0,1,0,1,0,1,0,0, 0.5f,0.5f,0.5f,0,1,0,1,0,1,0,0, -0.5f,0.5f,0.5f,0,1,0,0,0,1,0,0, -0.5f,0.5f,-0.5f,0,1,0,0,1,1,0,0 };
    float floorV[] = { -7,0,-7,0,1,0,0,5,1,0,0, 7,0,-7,0,1,0,5,5,1,0,0, 7,0,7,0,1,0,5,0,1,0,0, 7,0,7,0,1,0,5,0,1,0,0, -7,0,7,0,1,0,0,0,1,0,0, -7,0,-7,0,1,0,0,5,1,0,0,
                       -7,0,-7,0,0,1,0,0,1,0,0, 7,0,-7,0,0,1,5,0,1,0,0, 7,5,-7,0,0,1,5,2,1,0,0, 7,5,-7,0,0,1,5,2,1,0,0, -7,5,-7,0,0,1,0,2,1,0,0, -7,0,-7,0,0,1,0,0,1,0,0,
                       -7,0,7,1,0,0,0,0,0,0,-1, -7,0,-7,1,0,0,5,0,0,0,-1, -7,5,-7,1,0,0,5,2,0,0,-1, -7,5,-7,1,0,0,5,2,0,0,-1, -7,5,7,1,0,0,0,2,0,0,-1, -7,0,7,1,0,0,0,0,0,0,-1 };
    float skyV[] = { -1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,1,-1,1,-1,1,1,-1,1,1,1,1,1,1,-1,1,1,-1,1,-1,-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,-1,-1,-1,1,1,-1,1 };

    glGenVertexArrays(1, &cubeVAO); glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO); glBindBuffer(GL_ARRAY_BUFFER, cubeVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(cubeV), cubeV, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)(8*sizeof(float)));

    glGenVertexArrays(1, &floorVAO); glGenBuffers(1, &floorVBO);
    glBindVertexArray(floorVAO); glBindBuffer(GL_ARRAY_BUFFER, floorVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(floorV), floorV, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11*sizeof(float), (void*)(8*sizeof(float)));

    Geometry::setupSphere(sphereVAO, sphereVBO, sphereCount);
    Geometry::setupIcosahedron(icoVAO, icoVBO, icoCount);

    glGenVertexArrays(1, &skVAO); glGenBuffers(1, &skVBO);
    glBindVertexArray(skVAO); glBindBuffer(GL_ARRAY_BUFFER, skVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(skyV), skyV, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0); glEnableVertexAttribArray(0);

    quadVAO = 0;
}

void Renderer::renderQuad() {
    if (quadVAO == 0) {
        float quadVertices[] = { -1.0f,1.0f,0.0f,0.0f,1.0f, -1.0f,-1.0f,0.0f,0.0f,0.0f, 1.0f,1.0f,0.0f,1.0f,1.0f, 1.0f,-1.0f,0.0f,1.0f,0.0f };
        glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
    }
    glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); glBindVertexArray(0);
}

// -----------------------------------------------------

void Renderer::renderScene(Scene* scene, bool useNormalMap, float tessLevel, float explosionFactor, float pSpread, float pSize, float pCount) {
    // 0. Shadow Pass
    glm::vec3 sP(5,10,5), pP(-2,2,1);
    for(const auto& e : scene->entities){ if(e.name=="Main Sun") sP=e.position; if(e.name=="Point Light") pP=e.position; }

    glm::mat4 lProj=glm::ortho(-10.0f,10.0f,-10.0f,10.0f,1.0f,30.0f), lView=glm::lookAt(sP,glm::vec3(0),glm::vec3(0,1,0)), lSpace=lProj*lView;
    float far_p=25.0f; glm::mat4 pProj=glm::perspective(glm::radians(90.0f),1.0f,1.0f,far_p);
    std::vector<glm::mat4> pMats;
    pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(1,0,0),glm::vec3(0,-1,0))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(-1,0,0),glm::vec3(0,-1,0)));
    pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,1,0),glm::vec3(0,0,1))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,-1,0),glm::vec3(0,0,-1)));
    pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,0,1),glm::vec3(0,-1,0))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,0,-1),glm::vec3(0,-1,0)));

    Shader* shadowShader = ResourceManager::getShader("shadow");
    Shader* advShadowShader = ResourceManager::getShader("advShadow");
    Shader* pointShadowShader = ResourceManager::getShader("pointShadow");
    Shader* advPointShadowShader = ResourceManager::getShader("advPointShadow");

    glViewport(0,0,SHADOW_WIDTH,SHADOW_HEIGHT); glBindFramebuffer(GL_FRAMEBUFFER, depthFBO); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
    shadowShader->use(); shadowShader->setMat4("lightSpaceMatrix", lSpace);
    renderEntitiesToGBuffer(shadowShader, scene->entities, false);
    advShadowShader->use(); advShadowShader->setMat4("lightSpaceMatrix", lSpace);
    advShadowShader->setFloat("tessLevel", tessLevel); advShadowShader->setFloat("explosionFactor", explosionFactor);
    renderEntitiesToGBuffer(advShadowShader, scene->entities, false);
    glCullFace(GL_BACK); glDisable(GL_CULL_FACE); glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0,0,1024,1024); glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO); glClear(GL_DEPTH_BUFFER_BIT);
    pointShadowShader->use(); for(int i=0;i<6;i++) pointShadowShader->setMat4("shadowMatrices["+std::to_string(i)+"]", pMats[i]);
    pointShadowShader->setFloat("far_plane", far_p); pointShadowShader->setVec3("lightPos", pP);
    renderEntitiesToGBuffer(pointShadowShader, scene->entities, false);
    advPointShadowShader->use(); for(int i=0;i<6;i++) advPointShadowShader->setMat4("shadowMatrices["+std::to_string(i)+"]", pMats[i]);
    advPointShadowShader->setFloat("far_plane", far_p); advPointShadowShader->setVec3("lightPos", pP);
    advPointShadowShader->setFloat("tessLevel", tessLevel); advPointShadowShader->setFloat("explosionFactor", explosionFactor);
    renderEntitiesToGBuffer(advPointShadowShader, scene->entities, false);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 1. Geometry Pass (G-Buffer)
    glViewport(0,0,SCR_WIDTH,SCR_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Shader* gbufferShader = ResourceManager::getShader("gbuffer");
    gbufferShader->use();
    gbufferShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
    gbufferShader->setMat4("view", scene->camera.GetViewMatrix());
    renderEntitiesToGBuffer(gbufferShader, scene->entities, useNormalMap);

    Shader* advGbufferShader = ResourceManager::getShader("advGbuffer");
    advGbufferShader->use();
    advGbufferShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
    advGbufferShader->setMat4("view", scene->camera.GetViewMatrix());
    advGbufferShader->setFloat("tessLevel", tessLevel);
    advGbufferShader->setFloat("explosionFactor", explosionFactor);
    renderEntitiesToGBuffer(advGbufferShader, scene->entities, useNormalMap);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. Deferred Lighting Pass
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Shader* defLightShader = ResourceManager::getShader("deferred_lighting");
    defLightShader->use();
    defLightShader->setVec3("viewPos", scene->camera.Position);
    defLightShader->setMat4("lightSpaceMatrix", lSpace);
    defLightShader->setFloat("far_plane", far_p);

    // Map light arrays
    int pointLightIdx = 0;
    for(const auto& e : scene->entities){
        if(!e.isLight || !e.visible) continue;
        if(e.name == "Main Sun") {
            defLightShader->setVec3("dirLight.position", e.position);
            defLightShader->setVec3("dirLight.color", e.lightColor * e.lightIntensity);
        } else {
            if(pointLightIdx < 32){
                defLightShader->setVec3("pointLights[" + std::to_string(pointLightIdx) + "].position", e.position);
                defLightShader->setVec3("pointLights[" + std::to_string(pointLightIdx) + "].color", e.lightColor * e.lightIntensity);
                defLightShader->setInt("pointLights[" + std::to_string(pointLightIdx) + "].castShadow", (e.name == "Point Light" ? 1 : 0));
                pointLightIdx++;
            }
        }
    }
    defLightShader->setInt("numPointLights", pointLightIdx);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosition);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, gPBR);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, depthMap);
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("irradianceMap"));
    glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("prefilterMap"));
    glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("brdfLUT"));
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

    renderQuad();

    // 3. Forward Pass (Depth blit + Transparents + Unlit items)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Skybox
    glDepthFunc(GL_LEQUAL); 
    Shader* skyboxShader = ResourceManager::getShader("skybox");
    skyboxShader->use();
    skyboxShader->setMat4("view", glm::mat4(glm::mat3(scene->camera.GetViewMatrix()))); 
    skyboxShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom), (float)SCR_WIDTH/SCR_HEIGHT, 0.1f, 100.0f));
    glBindVertexArray(skVAO); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("skyboxMap")); glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Forward Water
    Shader* forwardWaterShader = ResourceManager::getShader("forward_water");
    forwardWaterShader->use();
    forwardWaterShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
    forwardWaterShader->setMat4("view", scene->camera.GetViewMatrix());
    forwardWaterShader->setVec3("viewPos", scene->camera.Position);
    forwardWaterShader->setFloat("time", (float)glfwGetTime());
    // Lights for water
    for(const auto& e : scene->entities){
        if(!e.isLight || !e.visible) continue;
        if(e.name == "Main Sun") {
            forwardWaterShader->setVec3("light1.position", e.position);
            forwardWaterShader->setVec3("light1.color", e.lightColor * e.lightIntensity);
        } else if(e.name == "Point Light") {
            forwardWaterShader->setVec3("light2.position", e.position);
            forwardWaterShader->setVec3("light2.color", e.lightColor * e.lightIntensity);
        }
    }
    // Water textures
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthMap);
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("irradianceMap"));
    glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("prefilterMap"));
    glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("brdfLUT"));
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

    // Hack: Reuse renderEntitiesToGBuffer but with WATER type only trick if we override the shader's types,
    // actually it's easier to just write a short loop here
    unsigned int floorDiff = ResourceManager::getTexture("floorDiff");
    unsigned int waterNorm = ResourceManager::getTexture("waterNorm");
    for (const auto& e : scene->entities) {
        if (!e.visible || e.type != WATER) continue;
        forwardWaterShader->setMat4("model", e.getModelMatrix());
        forwardWaterShader->setFloat("roughness", e.roughness);
        forwardWaterShader->setFloat("metallic", e.metallic);
        forwardWaterShader->setFloat("material.ambientStrength", e.ambient);
        forwardWaterShader->setVec3("objectColor", e.color);
        forwardWaterShader->setFloat("reflectivity", e.reflectivity);
        forwardWaterShader->setBool("isWater", true);
        
        glm::mat4 texMat(1.0f);
        if (e.dynamicTexture) texMat = glm::translate(texMat, glm::vec3((float)glfwGetTime() * e.texSpeed, 0.0f, 0.0f));
        forwardWaterShader->setMat4("textureMatrix", texMat);
        
        glBindVertexArray(floorVAO); 
        glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, floorDiff);
        glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, waterNorm);
        glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));      
        glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));      
        glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
        glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("blackTex"));
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Unlit entities (Lights)
    Shader* unlitShader = ResourceManager::getShader("unlit");
    unlitShader->use();
    unlitShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
    unlitShader->setMat4("view", scene->camera.GetViewMatrix());
    for (const auto& e : scene->entities) {
        if (e.isLight && e.visible) {
            unlitShader->setMat4("model", e.getModelMatrix());
            unlitShader->setVec3("objectColor", e.lightColor * e.lightIntensity);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    // Particles
    Shader* particleShader = ResourceManager::getShader("particle");
    particleShader->use();
    particleShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f));
    particleShader->setMat4("view", scene->camera.GetViewMatrix());
    renderParticles(particleShader, scene->entities, (float)glfwGetTime(), pSpread, pSize, pCount);

    glDisable(GL_BLEND);
}

void Renderer::renderEntitiesToGBuffer(Shader* shader, const std::vector<Entity>& entities, bool useNormalMap) {
    shader->setBool("useNormalMap", useNormalMap);
    
    unsigned int whiteTex = ResourceManager::getTexture("whiteTex");
    unsigned int flatNormalTex = ResourceManager::getTexture("flatNormalTex");
    unsigned int blackTex = ResourceManager::getTexture("blackTex");

    for (const auto& e : entities) {
        if (!e.visible || e.type == WATER || e.type == PARTICLE || e.isLight) continue;
        
        if (shader->hasTessellation) { if (e.type != ADV_SPHERE) continue; }
        else { if (e.type == ADV_SPHERE || e.name == "Particle Source") continue; }
        
        shader->setMat4("model", e.getModelMatrix());
        shader->setFloat("roughness", e.roughness);
        shader->setFloat("metallic", e.metallic);
        shader->setFloat("material.ambientStrength", e.ambient);
        shader->setVec3("objectColor", e.color);
        shader->setFloat("reflectivity", e.reflectivity);

        // Remove dynamic texture logic string entirely if it's unused or simply translate
        glm::mat4 texMat(1.0f);
        if (e.dynamicTexture) texMat = glm::translate(texMat, glm::vec3((float)glfwGetTime() * e.texSpeed, 0.0f, 0.0f));
        shader->setMat4("textureMatrix", texMat);

        glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, whiteTex);      
        glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, flatNormalTex); 
        glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, whiteTex);      
        glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, whiteTex);      
        glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, whiteTex);
        glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, blackTex);

        if (e.type == FLOOR) {
            glBindVertexArray(floorVAO); 
            glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("floorDiff"));
            glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("floorNorm"));
            glDrawArrays(GL_TRIANGLES, 0, 18);
        } else if (e.type == CUBE) {
            glBindVertexArray(cubeVAO);
            glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("texDiff"));
            glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("texNorm"));
            glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("texSpec")); 
            glDrawArrays(GL_TRIANGLES, 0, 36);
        } else if (e.type == SPHERE || e.type == ICOSAHEDRON) {
            unsigned int vao = (e.type == SPHERE) ? sphereVAO : icoVAO;
            int count = (e.type == SPHERE) ? sphereCount : icoCount;
            glBindVertexArray(vao); 
            if (e.type == SPHERE) glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
            else glDrawArrays(GL_TRIANGLES, 0, count);
        } else if (e.type == ADV_SPHERE) {
            glBindVertexArray(icoVAO); 
            glPatchParameteri(GL_PATCH_VERTICES, 3);
            glDrawArrays(GL_PATCHES, 0, icoCount);
        } else if (e.type == MODEL && e.model) {
            e.model->Draw(*shader);
        }
    }
}

void Renderer::renderParticles(Shader* shader, const std::vector<Entity>& entities, float time, float spread, float size, float particleCount) {
    shader->setFloat("time", time); shader->setFloat("spread", spread); shader->setFloat("size", size); shader->setFloat("particleCount", particleCount);
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
            shader->setVec3("objectColor", e.color);
            shader->setMat4("model", e.getModelMatrix());
            glDrawArrays(GL_PATCHES, 0, 1);
        }
    }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
}
