#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <vector>
#include "Shader.h"
#include "Entity.h"
#include "Scene.h"
#include "ResourceManager.h"

class Renderer {
public:
    Renderer(unsigned int scrWidth, unsigned int scrHeight);
    ~Renderer();
    
    void renderScene(Scene* scene, bool useNormalMap, float tessLevel, float explosionFactor, float pSpread, float pSize, float pCount, bool multiView = false);
    
private:
    unsigned int SCR_WIDTH, SCR_HEIGHT;
    unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

    // G-Buffer
    unsigned int gBuffer;
    unsigned int gPosition, gNormal, gAlbedoSpec, gPBR, gEmissive;
    unsigned int rboDepth;
    
    // Shadows
    unsigned int depthFBO, depthMap;
    unsigned int pointShadowFBO, depthCubemap;

    // Geometry VAOs
    unsigned int cubeVAO, cubeVBO;
    unsigned int floorVAO, floorVBO;
    unsigned int sphereVAO, sphereVBO;
    int sphereCount;
    unsigned int icoVAO, icoVBO;
    int icoCount;
    unsigned int skVAO, skVBO;
    unsigned int quadVAO, quadVBO;

    void setupGBuffer();
    void setupShadows();
    void setupGeometry();
    
    void renderGeometryPass(Scene* scene, bool useNormalMap, float tessLevel, float explosionFactor);
    void renderLightingPass(Scene* scene);
    void renderForwardPass(Scene* scene, bool useNormalMap, float pSpread, float pSize, float pCount);
    
    void renderEntitiesToGBuffer(Shader* shader, const std::vector<Entity>& entities, bool useNormalMap, 
                                 glm::mat4 proj = glm::mat4(1.0f), glm::mat4 view = glm::mat4(1.0f),
                                 glm::vec3 lightPos = glm::vec3(0.0f), float far_plane = 0.0f, 
                                 const std::vector<glm::mat4>* shadowMatrices = nullptr);
    void renderParticles(Shader* shader, const std::vector<Entity>& entities, float time, float spread, float size, float particleCount);
    
    void renderQuad();
};

#endif
