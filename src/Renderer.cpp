#include "Renderer.h"
#include "WaterWaves.h"
#include "RippleSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include "ResourceManager.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include "Geometry.h"
#include "Model.h"

Renderer::Renderer(unsigned int scrWidth, unsigned int scrHeight) 
    : SCR_WIDTH(scrWidth), SCR_HEIGHT(scrHeight) {
    waterGridVAO = 0;
    waterGridVBO = 0;
    waterGridEBO = 0;
    waterGridIndexCount = 0;
    setupGBuffer();
    setupShadows();
    setupGeometry();
    
    glGenQueries(1, &queryShadow);
    glGenQueries(1, &queryGeometry);
    glGenQueries(1, &queryIBL);
    glGenQueries(1, &queryWater);
    glGenQueries(1, &queryPost);
}

Renderer::~Renderer() {
    if (waterGridVAO != 0) glDeleteVertexArrays(1, &waterGridVAO);
    if (waterGridVBO != 0) glDeleteBuffers(1, &waterGridVBO);
    if (waterGridEBO != 0) glDeleteBuffers(1, &waterGridEBO);
    
    glDeleteQueries(1, &queryShadow);
    glDeleteQueries(1, &queryGeometry);
    glDeleteQueries(1, &queryIBL);
    glDeleteQueries(1, &queryWater);
    glDeleteQueries(1, &queryPost);
}

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

    glGenTextures(1, &gEmissive); glBindTexture(GL_TEXTURE_2D, gEmissive);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gEmissive, 0);

    unsigned int attachments[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
    glDrawBuffers(5, attachments);

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
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
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

    // Generate high-resolution subdivided water grid (128x128 vertices)
    {
        int N = 128;
        std::vector<float> gridVertices;
        std::vector<unsigned int> gridIndices;
        gridVertices.reserve(N * N * 11);
        gridIndices.reserve((N - 1) * (N - 1) * 6);
        
        for (int z = 0; z < N; ++z) {
            float fz = (float)z / (float)(N - 1);
            float pz = -7.0f + 14.0f * fz;
            for (int x = 0; x < N; ++x) {
                float fx = (float)x / (float)(N - 1);
                float px = -7.0f + 14.0f * fx;
                
                // Position
                gridVertices.push_back(px);
                gridVertices.push_back(0.0f);
                gridVertices.push_back(pz);
                
                // Normal
                gridVertices.push_back(0.0f);
                gridVertices.push_back(1.0f);
                gridVertices.push_back(0.0f);
                
                // TexCoords
                gridVertices.push_back(fx * 5.0f);
                gridVertices.push_back(fz * 5.0f);
                
                // Tangent
                gridVertices.push_back(1.0f);
                gridVertices.push_back(0.0f);
                gridVertices.push_back(0.0f);
            }
        }
        
        for (int z = 0; z < N - 1; ++z) {
            for (int x = 0; x < N - 1; ++x) {
                unsigned int i0 = z * N + x;
                unsigned int i1 = i0 + 1;
                unsigned int i2 = (z + 1) * N + x;
                unsigned int i3 = i2 + 1;
                
                gridIndices.push_back(i0);
                gridIndices.push_back(i1);
                gridIndices.push_back(i2);
                
                gridIndices.push_back(i1);
                gridIndices.push_back(i3);
                gridIndices.push_back(i2);
            }
        }
        
        waterGridIndexCount = (int)gridIndices.size();
        
        glGenVertexArrays(1, &waterGridVAO);
        glGenBuffers(1, &waterGridVBO);
        glGenBuffers(1, &waterGridEBO);
        
        glBindVertexArray(waterGridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, waterGridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterGridEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, gridIndices.size() * sizeof(unsigned int), gridIndices.data(), GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
        
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
        
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
        
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
        
        glBindVertexArray(0);
    }

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

void Renderer::renderScene(Scene* scene, bool useNormalMap, float tessLevel, float explosionFactor, float pSpread, float pSize, float pCount, float shadowBias, float pcfRadius, bool multiView, bool debugBuoyancy, int gbufferVisualisationMode, bool waterWavesEnabled, int waterDebugMode) {
    // 0. Shadow Pass
    glBeginQuery(GL_TIME_ELAPSED, queryShadow);
    glm::vec3 sP(5,10,5), pP(-2,2,1);
    for(const auto& e : scene->entities){ if(e.name=="Main Sun") sP=e.position; if(e.name=="Point Light") pP=e.position; }

    // Fix degenerate lookAt when sun is directly above (direction parallel to up)
    glm::vec3 sunDir = glm::normalize(glm::vec3(0) - sP);
    glm::vec3 shadowUp = (glm::abs(glm::dot(sunDir, glm::vec3(0,1,0))) > 0.99f) ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
    glm::mat4 lProj=glm::ortho(-30.0f,30.0f,-30.0f,30.0f,0.5f,80.0f), lView=glm::lookAt(sP,glm::vec3(0),shadowUp), lSpace=lProj*lView;
    float far_p=30.0f; glm::mat4 pProj=glm::perspective(glm::radians(90.0f),1.0f,1.0f,far_p);
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
    advShadowShader->setBool("isShadowPass", true);
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
    glEndQuery(GL_TIME_ELAPSED);

    struct VP { int x, y, w, h; glm::mat4 view; glm::mat4 proj; glm::vec3 pos; };
    std::vector<VP> viewports;
    if (multiView) {
        float hw = SCR_WIDTH/2.0f; float hh = SCR_HEIGHT/2.0f;
        float aspect = hw / hh; // 960/540 = 16:9
        float orthoH = 15.0f; // vertical half-size
        float orthoW = orthoH * aspect; // horizontal half-size scaled to aspect ratio
        glm::mat4 oProj = glm::ortho(-orthoW, orthoW, -orthoH, orthoH, 0.1f, 100.0f);
        glm::mat4 pProj = glm::perspective(glm::radians(scene->camera.Zoom), hw/hh, 0.1f, 100.0f);
        viewports.push_back({0, (int)hh, (int)hw, (int)hh, glm::lookAt(glm::vec3(0, 30, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1)), oProj, glm::vec3(0, 30, 0)}); // Top
        viewports.push_back({(int)hw, (int)hh, (int)hw, (int)hh, scene->camera.GetViewMatrix(), pProj, scene->camera.Position}); // Perspective
        viewports.push_back({0, 0, (int)hw, (int)hh, glm::lookAt(glm::vec3(0, 10.5f, 10.4f), glm::vec3(0, 10.5f, 0), glm::vec3(0, 1, 0)), oProj, glm::vec3(0, 10.5f, 10.4f)}); // Front
        viewports.push_back({(int)hw, 0, (int)hw, (int)hh, glm::lookAt(glm::vec3(10.4f, 10.5f, 0), glm::vec3(0, 10.5f, 0), glm::vec3(0, 1, 0)), oProj, glm::vec3(10.4f, 10.5f, 0)}); // Side
    } else {
        viewports.push_back({0, 0, (int)SCR_WIDTH, (int)SCR_HEIGHT, scene->camera.GetViewMatrix(), glm::perspective(glm::radians(scene->camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,500.0f), scene->camera.Position});
    }

    // 1. Geometry Pass (G-Buffer)
    glBeginQuery(GL_TIME_ELAPSED, queryGeometry);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Shader* gbufferShader = ResourceManager::getShader("gbuffer");
    Shader* advGbufferShader = ResourceManager::getShader("advGbuffer");
    
    for (const auto& vp : viewports) {
        glViewport(vp.x, vp.y, vp.w, vp.h);
        gbufferShader->use();
        gbufferShader->setMat4("projection", vp.proj); gbufferShader->setMat4("view", vp.view);
        renderEntitiesToGBuffer(gbufferShader, scene->entities, useNormalMap);
        advGbufferShader->use();
        advGbufferShader->setMat4("projection", vp.proj); advGbufferShader->setMat4("view", vp.view);
        advGbufferShader->setFloat("tessLevel", tessLevel); advGbufferShader->setFloat("explosionFactor", explosionFactor);
        renderEntitiesToGBuffer(advGbufferShader, scene->entities, useNormalMap);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEndQuery(GL_TIME_ELAPSED);

    // 2. Deferred Lighting Pass
    glBeginQuery(GL_TIME_ELAPSED, queryIBL);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Shader* defLightShader = ResourceManager::getShader("deferred_lighting");
    defLightShader->use();
    defLightShader->setMat4("lightSpaceMatrix", lSpace);
    defLightShader->setFloat("far_plane", far_p);
    defLightShader->setFloat("u_shadowBias", shadowBias);
    defLightShader->setFloat("u_pcfRadius", pcfRadius);
    defLightShader->setInt("u_visualisationMode", gbufferVisualisationMode);

    // Reset dirLight to zero so missing/invisible Main Sun doesn't retain old values
    defLightShader->setVec3("dirLight.color", glm::vec3(0.0f));
    
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
    glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_2D, gEmissive);

    for (const auto& vp : viewports) {
        glViewport(vp.x, vp.y, vp.w, vp.h);
        defLightShader->setVec3("viewPos", vp.pos);
        renderQuad();
    }
    glEndQuery(GL_TIME_ELAPSED);

    // 3. Forward Pass (Depth blit + Transparents + Unlit items)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Shader* skyboxShader = ResourceManager::getShader("skybox");
    Shader* forwardWaterShader = ResourceManager::getShader("forward_water");
    Shader* unlitShader = ResourceManager::getShader("unlit");
    Shader* particleShader = ResourceManager::getShader("particle");
    
    for (const auto& vp : viewports) {
        glViewport(vp.x, vp.y, vp.w, vp.h);
        
        bool isFirstViewport = (&vp == &viewports[0]);

        glDepthFunc(GL_LEQUAL); 
        skyboxShader->use();
        skyboxShader->setMat4("view", glm::mat4(glm::mat3(vp.view))); 
        skyboxShader->setMat4("projection", vp.proj);
        glBindVertexArray(skVAO); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("skyboxMap")); glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        if (isFirstViewport) glBeginQuery(GL_TIME_ELAPSED, queryPost);
        unlitShader->use();
        unlitShader->setMat4("projection", vp.proj); unlitShader->setMat4("view", vp.view);
        for (const auto& e : scene->entities) {
            if (e.isLight && e.visible) {
                unlitShader->setMat4("model", e.getModelMatrix()); unlitShader->setVec3("objectColor", e.lightColor * e.lightIntensity);
                glBindVertexArray(cubeVAO); glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

        particleShader->use();
        particleShader->setMat4("projection", vp.proj); particleShader->setMat4("view", vp.view);
        renderParticles(particleShader, scene->entities, (float)glfwGetTime(), pSpread, pSize, pCount);
        if (isFirstViewport) glEndQuery(GL_TIME_ELAPSED);

        if (isFirstViewport) glBeginQuery(GL_TIME_ELAPSED, queryWater);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        forwardWaterShader->use();
        forwardWaterShader->setMat4("projection", vp.proj);
        forwardWaterShader->setMat4("view", vp.view);
        forwardWaterShader->setVec3("viewPos", vp.pos);
        forwardWaterShader->setVec2("u_screenSize", glm::vec2((float)SCR_WIDTH, (float)SCR_HEIGHT));
        forwardWaterShader->setInt("u_waterDebugMode", waterDebugMode);
        forwardWaterShader->setFloat("time", (float)glfwGetTime());
        forwardWaterShader->setFloat("u_shadowBias", shadowBias);
        forwardWaterShader->setFloat("u_pcfRadius", pcfRadius);
        forwardWaterShader->setBool("u_debugRippleHeatmap", debugBuoyancy);
        forwardWaterShader->setBool("u_enableWaterWaves", waterWavesEnabled);
        
        setWaterWaveUniforms(*forwardWaterShader);
        RippleSystem::bindRippleUniforms(*forwardWaterShader, 9, true);

        // Reset water light uniforms to zero before the loop
        // so invisible/removed lights properly become dark
        forwardWaterShader->setVec3("light1.color", glm::vec3(0.0f));
        forwardWaterShader->setVec3("light2.color", glm::vec3(0.0f));
        const Entity* fallbackWaterPointLight = nullptr;
        const Entity* highlightWaterPointLight = nullptr;
        // Lights for water
        for(const auto& e : scene->entities){
            if(!e.isLight || !e.visible) continue;
            if(e.name == "Main Sun") {
                forwardWaterShader->setVec3("light1.position", e.position);
                forwardWaterShader->setVec3("light1.color", e.lightColor * e.lightIntensity);
            } else if(e.name == "Water Highlight Light") {
                highlightWaterPointLight = &e;
            } else if(e.name == "Point Light" && fallbackWaterPointLight == nullptr) {
                fallbackWaterPointLight = &e;
            }
        }
        const Entity* waterPointLight = highlightWaterPointLight ? highlightWaterPointLight : fallbackWaterPointLight;
        if (waterPointLight) {
            forwardWaterShader->setVec3("light2.position", waterPointLight->position);
            forwardWaterShader->setVec3("light2.color", waterPointLight->lightColor * waterPointLight->lightIntensity);
            forwardWaterShader->setBool("u_light2CastsShadow", waterPointLight->name == "Point Light");
        } else {
            forwardWaterShader->setBool("u_light2CastsShadow", false);
        }
        forwardWaterShader->setMat4("lightSpaceMatrix", lSpace);
        forwardWaterShader->setFloat("far_plane", far_p);

        const Entity* openBoxClipEntity = nullptr;
        for (const auto& e : scene->entities) {
            if (e.visible && e.name == "Open Box" && e.buoyancyType == 2) {
                openBoxClipEntity = &e;
                break;
            }
        }
        constexpr float openBoxWallThickness = 0.05f;
        constexpr float internalWaterThreshold = 0.025f;
        float openBoxFloodLevel = openBoxClipEntity ? glm::clamp(openBoxClipEntity->floodLevel, 0.0f, 1.0f) : 0.0f;
        forwardWaterShader->setBool("u_openBoxClipEnabled", openBoxClipEntity != nullptr);
        forwardWaterShader->setFloat("u_openBoxWallThickness", openBoxWallThickness);
        forwardWaterShader->setFloat("u_openBoxFloodLevel", openBoxFloodLevel);
        forwardWaterShader->setFloat("u_internalWaterLocalHeight", -0.5f + openBoxWallThickness);
        if (openBoxClipEntity) {
            forwardWaterShader->setMat4("u_openBoxInverseModel", glm::inverse(openBoxClipEntity->getModelMatrix()));
        } else {
            forwardWaterShader->setMat4("u_openBoxInverseModel", glm::mat4(1.0f));
        }

        // Water textures
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthMap);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("irradianceMap"));
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, ResourceManager::getTexture("prefilterMap"));
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("brdfLUT"));
        glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
        unsigned int floorDiff = ResourceManager::getTexture("floorDiff");
        unsigned int waterNorm = ResourceManager::getTexture("waterNorm");
        for (const auto& e : scene->entities) {
            if (!e.visible || e.type != WATER) continue;
            forwardWaterShader->setMat4("model", e.getModelMatrix());
            forwardWaterShader->setFloat("roughness", e.roughness); forwardWaterShader->setFloat("metallic", e.metallic);
            forwardWaterShader->setFloat("material.ambientStrength", e.ambient); forwardWaterShader->setVec3("objectColor", e.color);
            
            // Animate water reflectivity with wave amplitude for spray at peaks
            float peakFactor = 0.5f * (1.0f + glm::sin(3.0f * (float)glfwGetTime()));
            forwardWaterShader->setFloat("reflectivity", e.reflectivity * (1.0f + 0.5f * peakFactor));
            
            forwardWaterShader->setBool("isWater", true);
            forwardWaterShader->setBool("u_isInternalBoxWater", false);
            glm::mat4 texMat(1.0f); if(e.dynamicTexture) texMat=glm::translate(texMat, glm::vec3((float)glfwGetTime()*e.texSpeed, 0.0f, 0.0f));
            forwardWaterShader->setMat4("textureMatrix", texMat);
            
            if (waterGridVAO != 0) {
                glBindVertexArray(waterGridVAO); 
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, floorDiff); glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, waterNorm);
                glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex")); glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));      
                glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex")); glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("blackTex"));
                glDrawElements(GL_TRIANGLES, waterGridIndexCount, GL_UNSIGNED_INT, 0);
            } else {
                glBindVertexArray(floorVAO); 
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, floorDiff); glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, waterNorm);
                glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex")); glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));      
                glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex")); glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("blackTex"));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        if (openBoxClipEntity && openBoxFloodLevel > internalWaterThreshold) {
            float innerSpan = 1.0f - 2.0f * openBoxWallThickness;
            float localBottom = -0.5f + openBoxWallThickness + 0.004f;
            float localTop = 0.5f - 0.012f;
            float localWaterY = glm::mix(localBottom, localTop, openBoxFloodLevel);
            glm::mat4 internalWaterModel = openBoxClipEntity->getModelMatrix();
            internalWaterModel = glm::translate(internalWaterModel, glm::vec3(0.0f, localWaterY, 0.0f));
            internalWaterModel = glm::scale(internalWaterModel, glm::vec3(innerSpan / 14.0f, 1.0f, innerSpan / 14.0f));

            forwardWaterShader->setMat4("model", internalWaterModel);
            forwardWaterShader->setFloat("roughness", 0.08f);
            forwardWaterShader->setFloat("metallic", 0.0f);
            forwardWaterShader->setFloat("material.ambientStrength", 1.0f);
            forwardWaterShader->setVec3("objectColor", glm::vec3(0.05f, 0.32f, 0.72f));
            forwardWaterShader->setFloat("reflectivity", 0.30f + 0.30f * openBoxFloodLevel);
            forwardWaterShader->setBool("isWater", true);
            forwardWaterShader->setBool("u_isInternalBoxWater", true);
            forwardWaterShader->setBool("u_openBoxClipEnabled", false);
            forwardWaterShader->setFloat("u_internalWaterLocalHeight", localWaterY);
            forwardWaterShader->setMat4("textureMatrix", glm::mat4(1.0f));

            if (waterGridVAO != 0) {
                glBindVertexArray(waterGridVAO);
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, floorDiff);
                glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, waterNorm);
                glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
                glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
                glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
                glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("blackTex"));
                glDrawElements(GL_TRIANGLES, waterGridIndexCount, GL_UNSIGNED_INT, 0);
            } else {
                glBindVertexArray(floorVAO);
                glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, floorDiff);
                glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, waterNorm);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            forwardWaterShader->setBool("u_isInternalBoxWater", false);
            forwardWaterShader->setBool("u_openBoxClipEnabled", openBoxClipEntity != nullptr);
        }
        glDepthMask(GL_TRUE);

        // Transparent glass tank overlay. Keep depth testing, but do not write depth,
        // so the glass tint never hides the water surface or submerged objects.
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        forwardWaterShader->use();
        for (const auto& e : scene->entities) {
            if (!e.visible || (e.name != "Tank Bottom" && e.name != "Tank Left" && e.name != "Tank Right" && e.name != "Tank Back" && e.name != "Tank Front")) continue;
            forwardWaterShader->setMat4("model", e.getModelMatrix());
            forwardWaterShader->setFloat("roughness", e.roughness);
            forwardWaterShader->setFloat("metallic", e.metallic);
            forwardWaterShader->setFloat("material.ambientStrength", e.ambient);
            forwardWaterShader->setVec3("objectColor", e.color);
            forwardWaterShader->setFloat("reflectivity", e.reflectivity);
            forwardWaterShader->setBool("isWater", false);
            forwardWaterShader->setBool("useNormalMap", false);
            forwardWaterShader->setMat4("textureMatrix", glm::mat4(1.0f));

            glBindVertexArray(cubeVAO);
            glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
            glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("flatNormalTex"));
            glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
            glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
            glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("whiteTex"));
            glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, ResourceManager::getTexture("blackTex"));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glDepthMask(GL_TRUE);

        if (openBoxClipEntity && waterDebugMode >= 8) {
            glm::mat4 boxModel = openBoxClipEntity->getModelMatrix();
            float inner = 0.5f - openBoxWallThickness;
            float bottomY = -0.5f + openBoxWallThickness;
            float topY = 0.5f;
            float waterY = glm::mix(bottomY + 0.004f, topY - 0.012f, openBoxFloodLevel);
            glm::vec3 maskColor = glm::vec3(0.0f, 1.0f, 0.25f);
            glm::vec3 internalColor = glm::vec3(0.0f, 0.45f, 1.0f);
            glm::vec3 invalidColor = glm::vec3(1.0f, 0.05f, 0.02f);
            glm::vec3 outlineColor = (waterDebugMode == 10) ? invalidColor : ((openBoxFloodLevel > internalWaterThreshold) ? internalColor : maskColor);

            auto toWorld = [&](glm::vec3 p) {
                return glm::vec3(boxModel * glm::vec4(p, 1.0f));
            };
            auto drawRect = [&](float y, glm::vec3 color) {
                glm::vec3 a = toWorld(glm::vec3(-inner, y, -inner));
                glm::vec3 b = toWorld(glm::vec3( inner, y, -inner));
                glm::vec3 c = toWorld(glm::vec3( inner, y,  inner));
                glm::vec3 d = toWorld(glm::vec3(-inner, y,  inner));
                drawDebugLine(a, b, color, vp.view, vp.proj);
                drawDebugLine(b, c, color, vp.view, vp.proj);
                drawDebugLine(c, d, color, vp.view, vp.proj);
                drawDebugLine(d, a, color, vp.view, vp.proj);
            };

            drawRect(bottomY, outlineColor);
            drawRect(topY, outlineColor);
            if (openBoxFloodLevel > internalWaterThreshold) {
                drawRect(waterY, internalColor);
            }
        }

        if (isFirstViewport) glEndQuery(GL_TIME_ELAPSED);

        // 4. Buoyancy Debug Overlay (Wireframe & Force Arrows)
        if (debugBuoyancy) {
            // A. Draw Tank wireframe (10 x 5 x 10 centered at (0, 2.5, 0))
            glm::vec3 c000(-5.0f, 0.0f, -5.0f), c100(5.0f, 0.0f, -5.0f), c101(5.0f, 0.0f, 5.0f), c001(-5.0f, 0.0f, 5.0f);
            glm::vec3 c010(-5.0f, 5.0f, -5.0f), c110(5.0f, 5.0f, -5.0f), c111(5.0f, 5.0f, 5.0f), c011(-5.0f, 5.0f, 5.0f);
            
            glm::vec3 tankColor(1.0f, 0.84f, 0.0f); // Sleek gold for tank wireframe!
            
            // Bottom square
            drawDebugLine(c000, c100, tankColor, vp.view, vp.proj);
            drawDebugLine(c100, c101, tankColor, vp.view, vp.proj);
            drawDebugLine(c101, c001, tankColor, vp.view, vp.proj);
            drawDebugLine(c001, c000, tankColor, vp.view, vp.proj);
            
            // Top square
            drawDebugLine(c010, c110, tankColor, vp.view, vp.proj);
            drawDebugLine(c110, c111, tankColor, vp.view, vp.proj);
            drawDebugLine(c111, c011, tankColor, vp.view, vp.proj);
            drawDebugLine(c011, c010, tankColor, vp.view, vp.proj);
            
            // Vertical pillars
            drawDebugLine(c000, c010, tankColor, vp.view, vp.proj);
            drawDebugLine(c100, c110, tankColor, vp.view, vp.proj);
            drawDebugLine(c101, c111, tankColor, vp.view, vp.proj);
            drawDebugLine(c001, c011, tankColor, vp.view, vp.proj);

            // B. Draw Buoyant Entities Wireframes and Force Arrows
            for (const auto& e : scene->entities) {
                if (!e.isBuoyant || !e.visible) continue;
                
                // Draw Wireframe on top of the shaded model
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                unlitShader->use();
                unlitShader->setMat4("projection", vp.proj);
                unlitShader->setMat4("view", vp.view);
                unlitShader->setMat4("model", e.getModelMatrix());
                
                // Wireframe color: interpolate Cyan -> Yellow -> Red based on floodLevel!
                glm::vec3 wireColor;
                if (e.floodLevel < 0.5f) {
                    float t = e.floodLevel * 2.0f;
                    wireColor = glm::mix(glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f), t);
                } else {
                    float t = (e.floodLevel - 0.5f) * 2.0f;
                    wireColor = glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.2f, 0.2f), t);
                }
                unlitShader->setVec3("objectColor", wireColor);
                
                if (e.buoyancyType == 0) { // Sphere
                    glBindVertexArray(sphereVAO);
                    glDrawElements(GL_TRIANGLES, sphereCount, GL_UNSIGNED_INT, 0);
                } else { // Cube (Slab or Open Box)
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                // C. Draw Force Arrows
                float forceScale = 0.002f; 
                
                // Gravity Force (acts at CM e.position): red
                glm::vec3 gravityVec(0.0f, -e.mass * 9.81f, 0.0f);
                drawDebugArrow(e.position, e.position + gravityVec * forceScale, glm::vec3(1.0f, 0.0f, 0.0f), vp.view, vp.proj);
                
                // Buoyancy Force (acts at CB e.buoyancyCenter): blue
                if (glm::length(e.buoyancyForce) > 0.001f) {
                    drawDebugArrow(e.buoyancyCenter, e.buoyancyCenter + e.buoyancyForce * forceScale, glm::vec3(0.0f, 0.0f, 1.0f), vp.view, vp.proj);
                }
                
                // Drag Force (acts at CM e.position): yellow
                glm::vec3 dragForce = e.waterDragForce;
                if (glm::length(dragForce) > 0.001f) {
                    drawDebugArrow(e.position, e.position + dragForce * forceScale, glm::vec3(1.0f, 1.0f, 0.0f), vp.view, vp.proj);
                }

                if (glm::length(e.dampingForce) > 0.001f) {
                    drawDebugArrow(e.position, e.position + e.dampingForce * forceScale, glm::vec3(0.7f, 0.25f, 1.0f), vp.view, vp.proj);
                }

                float torqueScale = 0.015f;
                if (glm::length(e.torqueDebug) > 0.001f) {
                    drawDebugArrow(e.position, e.position + glm::normalize(e.torqueDebug) * glm::min(glm::length(e.torqueDebug) * torqueScale, 1.25f), glm::vec3(1.0f, 0.45f, 0.85f), vp.view, vp.proj);
                }

                if (glm::length(e.angularVelocityDebug) > 0.001f) {
                    drawDebugArrow(e.position + glm::vec3(0.0f, 0.2f, 0.0f), e.position + glm::vec3(0.0f, 0.2f, 0.0f) + e.angularVelocityDebug * 0.18f, glm::vec3(0.35f, 1.0f, 0.2f), vp.view, vp.proj);
                }

                size_t wallContactCount = e.debugWallContactPoints.size();
                for (size_t i = 0; i < wallContactCount; ++i) {
                    glm::vec3 p = e.debugWallContactPoints[i];
                    glm::vec3 n = (i < e.debugWallContactNormals.size()) ? e.debugWallContactNormals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
                    glm::vec3 correction = (i < e.debugWallCorrectionVectors.size()) ? e.debugWallCorrectionVectors[i] : glm::vec3(0.0f);
                    glm::vec3 impulse = (i < e.debugWallImpulseVectors.size()) ? e.debugWallImpulseVectors[i] : glm::vec3(0.0f);
                    glm::vec3 tangent = (i < e.debugWallTangentialVelocities.size()) ? e.debugWallTangentialVelocities[i] : glm::vec3(0.0f);
                    float depth = (i < e.debugWallPenetrationDepths.size()) ? e.debugWallPenetrationDepths[i] : 0.0f;
                    float normalImpulse = (i < e.debugWallNormalImpulses.size()) ? e.debugWallNormalImpulses[i] : 0.0f;
                    float frictionImpulse = (i < e.debugWallFrictionImpulses.size()) ? e.debugWallFrictionImpulses[i] : 0.0f;

                    float marker = 0.045f + glm::clamp(depth, 0.0f, 0.08f);
                    glm::vec3 contactColor = depth > 0.010f ? glm::vec3(1.0f, 0.05f, 0.03f) : glm::vec3(0.1f, 1.0f, 0.25f);
                    drawDebugLine(p - glm::vec3(marker, 0.0f, 0.0f), p + glm::vec3(marker, 0.0f, 0.0f), contactColor, vp.view, vp.proj);
                    drawDebugLine(p - glm::vec3(0.0f, marker, 0.0f), p + glm::vec3(0.0f, marker, 0.0f), contactColor, vp.view, vp.proj);
                    drawDebugLine(p - glm::vec3(0.0f, 0.0f, marker), p + glm::vec3(0.0f, 0.0f, marker), contactColor, vp.view, vp.proj);

                    drawDebugArrow(p, p + n * (0.20f + glm::clamp(depth * 5.0f, 0.0f, 0.35f)), glm::vec3(0.05f, 0.25f, 1.0f), vp.view, vp.proj);
                    if (glm::length(correction) > 0.0001f) {
                        drawDebugArrow(p, p + correction * 6.0f, glm::vec3(0.1f, 1.0f, 0.25f), vp.view, vp.proj);
                    }
                    if (glm::length(impulse) > 0.0001f) {
                        drawDebugArrow(p, p + glm::normalize(impulse) * glm::clamp(normalImpulse * 0.04f, 0.05f, 0.45f), glm::vec3(1.0f, 0.85f, 0.05f), vp.view, vp.proj);
                    }
                    if (glm::length(tangent) > 0.0001f) {
                        drawDebugArrow(p, p + glm::normalize(tangent) * glm::clamp(glm::length(tangent) * 0.08f + frictionImpulse * 0.03f, 0.04f, 0.35f), glm::vec3(1.0f, 0.55f, 0.05f), vp.view, vp.proj);
                    }
                }
                
                // Lever arm connection (thin line between CM and CB): gray
                drawDebugLine(e.position, e.buoyancyCenter, glm::vec3(0.5f, 0.5f, 0.5f), vp.view, vp.proj);

                // Water query diagnostics: object sample, displaced water point, and queried normal.
                size_t sampleCount = std::min(e.debugBuoyancySamples.size(), e.debugWaterSurfacePoints.size());
                size_t stride = sampleCount > 80 ? (sampleCount / 80 + 1) : 1;
                for (size_t i = 0; i < sampleCount; i += stride) {
                    glm::vec3 sample = e.debugBuoyancySamples[i];
                    glm::vec3 rawWaterPoint = (i < e.debugRawWaterSurfacePoints.size()) ? e.debugRawWaterSurfacePoints[i] : e.debugWaterSurfacePoints[i];
                    glm::vec3 waterPoint = e.debugWaterSurfacePoints[i];
                    glm::vec3 normal = (i < e.debugWaterSurfaceNormals.size()) ? e.debugWaterSurfaceNormals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
                    float submergedDepth = (i < e.debugWaterSubmergedDepths.size()) ? e.debugWaterSubmergedDepths[i] : 0.0f;
                    glm::vec3 sampleColor = submergedDepth > 0.0f
                        ? glm::mix(glm::vec3(1.0f, 0.85f, 0.1f), glm::vec3(0.1f, 0.9f, 1.0f), glm::clamp(submergedDepth * 2.0f, 0.0f, 1.0f))
                        : glm::vec3(1.0f, 0.25f, 0.25f);

                    float marker = 0.035f;
                    drawDebugLine(sample - glm::vec3(marker, 0.0f, 0.0f), sample + glm::vec3(marker, 0.0f, 0.0f), sampleColor, vp.view, vp.proj);
                    drawDebugLine(sample - glm::vec3(0.0f, marker, 0.0f), sample + glm::vec3(0.0f, marker, 0.0f), sampleColor, vp.view, vp.proj);
                    drawDebugLine(sample - glm::vec3(0.0f, 0.0f, marker), sample + glm::vec3(0.0f, 0.0f, marker), sampleColor, vp.view, vp.proj);

                    drawDebugLine(sample, waterPoint, glm::vec3(0.9f, 0.9f, 0.9f), vp.view, vp.proj);
                    if (glm::abs(rawWaterPoint.y - waterPoint.y) > 0.002f) {
                        drawDebugLine(rawWaterPoint, waterPoint, glm::vec3(0.95f, 0.75f, 0.15f), vp.view, vp.proj);
                        float rawMarker = 0.025f;
                        drawDebugLine(rawWaterPoint - glm::vec3(rawMarker, 0.0f, 0.0f), rawWaterPoint + glm::vec3(rawMarker, 0.0f, 0.0f), glm::vec3(1.0f, 0.55f, 0.05f), vp.view, vp.proj);
                    }
                    drawDebugArrow(waterPoint, waterPoint + normal * 0.35f, glm::vec3(0.1f, 1.0f, 0.45f), vp.view, vp.proj);
                    if (submergedDepth > 0.0f) {
                        drawDebugArrow(waterPoint, waterPoint + normal * glm::min(submergedDepth, 0.4f), glm::vec3(0.2f, 0.8f, 1.0f), vp.view, vp.proj);
                    }
                }

                // Ripple contact patch diagnostics: pressure heatmap plus brush radius.
                size_t rippleSampleCount = e.debugRippleInjectionPoints.size();
                for (size_t i = 0; i < rippleSampleCount; ++i) {
                    glm::vec3 sample = e.debugRippleInjectionPoints[i];
                    float radius = (i < e.debugRippleInjectionRadii.size()) ? e.debugRippleInjectionRadii[i] : 0.25f;
                    float pressure = (i < e.debugRippleInjectionPressures.size()) ? e.debugRippleInjectionPressures[i] : 0.0f;
                    float strength = (i < e.debugRippleInjectionStrengths.size()) ? e.debugRippleInjectionStrengths[i] : 0.0f;
                    float pressureHeat = glm::clamp(pressure / (1000.0f * 9.81f * 0.75f), 0.0f, 1.0f);
                    float strengthHeat = glm::clamp(glm::abs(strength) / 0.080f, 0.12f, 1.0f);
                    glm::vec3 signColor = strength < 0.0f ? glm::vec3(1.0f, 0.08f, 0.03f) : glm::vec3(0.05f, 0.38f, 1.0f);
                    glm::vec3 pressureColor = signColor * glm::mix(0.35f, 1.0f, glm::max(pressureHeat, strengthHeat));
                    glm::vec3 p(sample.x, 4.035f, sample.z);
                    float marker = 0.045f + glm::clamp(glm::abs(strength) * 0.8f, 0.0f, 0.08f);

                    drawDebugLine(p - glm::vec3(marker, 0.0f, 0.0f), p + glm::vec3(marker, 0.0f, 0.0f), pressureColor, vp.view, vp.proj);
                    drawDebugLine(p - glm::vec3(0.0f, 0.0f, marker), p + glm::vec3(0.0f, 0.0f, marker), pressureColor, vp.view, vp.proj);

                    int segments = 16;
                    for (int s = 0; s < segments; ++s) {
                        float a0 = (float)s / (float)segments * glm::two_pi<float>();
                        float a1 = (float)(s + 1) / (float)segments * glm::two_pi<float>();
                        glm::vec3 p0(p.x + glm::cos(a0) * radius, p.y, p.z + glm::sin(a0) * radius);
                        glm::vec3 p1(p.x + glm::cos(a1) * radius, p.y, p.z + glm::sin(a1) * radius);
                        drawDebugLine(p0, p1, pressureColor, vp.view, vp.proj);
                    }
                }
            }

            for (const RippleImpulse& impulse : RippleSystem::instance().lastImpulses()) {
                glm::vec3 p = impulse.position;
                float r = impulse.radius;
                float heat = glm::clamp(glm::abs(impulse.strength) / 0.080f, 0.15f, 1.0f);
                glm::vec3 color = (impulse.strength < 0.0f ? glm::vec3(1.0f, 0.08f, 0.03f) : glm::vec3(0.05f, 0.38f, 1.0f)) * heat;
                int segments = 18;
                for (int i = 0; i < segments; ++i) {
                    float a0 = (float)i / (float)segments * glm::two_pi<float>();
                    float a1 = (float)(i + 1) / (float)segments * glm::two_pi<float>();
                    glm::vec3 p0 = glm::vec3(p.x + glm::cos(a0) * r, 4.02f, p.z + glm::sin(a0) * r);
                    glm::vec3 p1 = glm::vec3(p.x + glm::cos(a1) * r, 4.02f, p.z + glm::sin(a1) * r);
                    drawDebugLine(p0, p1, color, vp.view, vp.proj);
                }
                drawDebugArrow(glm::vec3(p.x, 4.02f, p.z), glm::vec3(p.x, 4.02f + impulse.strength * 4.0f, p.z), color, vp.view, vp.proj);
            }
        }
        glDisable(GL_BLEND);
    }

    // Retrieve query results asynchronously to prevent CPU-GPU pipeline stall
    GLuint64 timeNs = 0;
    GLint available = 0;

    glGetQueryObjectiv(queryShadow, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        glGetQueryObjectui64v(queryShadow, GL_QUERY_RESULT, &timeNs);
        timeShadow = timeNs / 1000000.0f;
    }

    glGetQueryObjectiv(queryGeometry, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        glGetQueryObjectui64v(queryGeometry, GL_QUERY_RESULT, &timeNs);
        timeGeometry = timeNs / 1000000.0f;
    }

    glGetQueryObjectiv(queryIBL, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        glGetQueryObjectui64v(queryIBL, GL_QUERY_RESULT, &timeNs);
        timeIBL = timeNs / 1000000.0f;
    }

    glGetQueryObjectiv(queryWater, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        glGetQueryObjectui64v(queryWater, GL_QUERY_RESULT, &timeNs);
        timeWater = timeNs / 1000000.0f;
    }

    glGetQueryObjectiv(queryPost, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        glGetQueryObjectui64v(queryPost, GL_QUERY_RESULT, &timeNs);
        timePost = timeNs / 1000000.0f;
    }
}

void Renderer::renderEntitiesToGBuffer(Shader* shader, const std::vector<Entity>& entities, bool useNormalMap) {
    shader->setBool("useNormalMap", useNormalMap);
    shader->setBool("isWater", false);
    shader->setBool("u_enableWaterWaves", false);
    shader->setBool("u_useRipple", false);
    
    unsigned int whiteTex = ResourceManager::getTexture("whiteTex");
    unsigned int flatNormalTex = ResourceManager::getTexture("flatNormalTex");
    unsigned int blackTex = ResourceManager::getTexture("blackTex");
    auto entityTexture = [](const std::string& textureName, const std::string& fallbackName) {
        if (!textureName.empty()) {
            unsigned int texture = ResourceManager::getTexture(textureName);
            if (texture != 0) return texture;
        }
        return ResourceManager::getTexture(fallbackName);
    };

    for (const auto& e : entities) {
        if (!e.visible || e.type == WATER || e.type == PARTICLE || e.isLight ||
            e.name == "Tank Bottom" || e.name == "Tank Left" || e.name == "Tank Right" || e.name == "Tank Back" || e.name == "Tank Front") continue;
        
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
            glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, entityTexture(e.albedoTexture, "texDiff"));
            glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, entityTexture(e.normalTexture, "texNorm"));
            glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, entityTexture(e.metallicTexture, "texSpec"));
            glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, entityTexture(e.roughnessTexture, "whiteTex"));
            glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, entityTexture(e.aoTexture, "whiteTex"));
            
            if (e.buoyancyType == 2) {
                // Hollow open-top box rendering: Draw 5 walls in local space
                float t = 0.05f; // Wall thickness (5% of scale)
                glm::mat4 baseModel = e.getModelMatrix();
                
                // 1. Bottom floor
                glm::mat4 m1 = glm::translate(baseModel, glm::vec3(0.0f, -0.5f + t * 0.5f, 0.0f));
                m1 = glm::scale(m1, glm::vec3(1.0f, t, 1.0f));
                shader->setMat4("model", m1);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                
                // 2. Left Wall (-X)
                glm::mat4 m2 = glm::translate(baseModel, glm::vec3(-0.5f + t * 0.5f, t * 0.5f, 0.0f));
                m2 = glm::scale(m2, glm::vec3(t, 1.0f - t, 1.0f));
                shader->setMat4("model", m2);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                
                // 3. Right Wall (+X)
                glm::mat4 m3 = glm::translate(baseModel, glm::vec3(0.5f - t * 0.5f, t * 0.5f, 0.0f));
                m3 = glm::scale(m3, glm::vec3(t, 1.0f - t, 1.0f));
                shader->setMat4("model", m3);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                
                // 4. Back Wall (-Z)
                glm::mat4 m4 = glm::translate(baseModel, glm::vec3(0.0f, t * 0.5f, -0.5f + t * 0.5f));
                m4 = glm::scale(m4, glm::vec3(1.0f - 2.0f * t, 1.0f - t, t));
                shader->setMat4("model", m4);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                
                // 5. Front Wall (+Z)
                glm::mat4 m5 = glm::translate(baseModel, glm::vec3(0.0f, t * 0.5f, 0.5f - t * 0.5f));
                m5 = glm::scale(m5, glm::vec3(1.0f - 2.0f * t, 1.0f - t, t));
                shader->setMat4("model", m5);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            } else {
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
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

void Renderer::drawDebugLine(glm::vec3 start, glm::vec3 end, glm::vec3 color, glm::mat4 view, glm::mat4 proj) {
    static unsigned int lineVAO = 0, lineVBO = 0;
    if (lineVAO == 0) {
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, 2 * 3 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }
    
    float vertices[] = {
        start.x, start.y, start.z,
        end.x, end.y, end.z
    };
    
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    
    Shader* debugLineShader = ResourceManager::getShader("debugLine");
    if (debugLineShader) {
        debugLineShader->use();
        debugLineShader->setMat4("view", view);
        debugLineShader->setMat4("projection", proj);
        debugLineShader->setVec3("objectColor", color);
        glDrawArrays(GL_LINES, 0, 2);
    }
    glBindVertexArray(0);
}

void Renderer::drawDebugArrow(glm::vec3 start, glm::vec3 end, glm::vec3 color, glm::mat4 view, glm::mat4 proj) {
    drawDebugLine(start, end, color, view, proj);
    
    glm::vec3 dir = end - start;
    float len = glm::length(dir);
    if (len < 0.001f) return;
    dir /= len;
    
    glm::vec3 right = (glm::abs(dir.y) > 0.99f) ? glm::vec3(1, 0, 0) : glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::normalize(glm::cross(right, dir));
    
    float arrowSize = glm::min(0.2f, len * 0.2f);
    glm::vec3 p1 = end - dir * arrowSize + right * (arrowSize * 0.5f);
    glm::vec3 p2 = end - dir * arrowSize - right * (arrowSize * 0.5f);
    glm::vec3 p3 = end - dir * arrowSize + up * (arrowSize * 0.5f);
    glm::vec3 p4 = end - dir * arrowSize - up * (arrowSize * 0.5f);
    
    drawDebugLine(end, p1, color, view, proj);
    drawDebugLine(end, p2, color, view, proj);
    drawDebugLine(end, p3, color, view, proj);
    drawDebugLine(end, p4, color, view, proj);
}
