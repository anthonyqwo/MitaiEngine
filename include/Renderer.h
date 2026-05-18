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
    
    void renderScene(Scene* scene, bool useNormalMap, float tessLevel, float explosionFactor, float pSpread, float pSize, float pCount, float shadowBias, float pcfRadius, bool multiView = false, bool debugBuoyancy = false, int gbufferVisualisationMode = 0, bool waterWavesEnabled = true, int waterDebugMode = 0);
    
    // Debug Drawing Utilities
    void drawDebugLine(glm::vec3 start, glm::vec3 end, glm::vec3 color, glm::mat4 view, glm::mat4 proj);
    void drawDebugArrow(glm::vec3 start, glm::vec3 end, glm::vec3 color, glm::mat4 view, glm::mat4 proj);

    // GPU Pass Timings (ms)
    float timeShadow = 0.0f;
    float timeGeometry = 0.0f;
    float timeIBL = 0.0f;
    float timeWater = 0.0f;
    float timePost = 0.0f;

private:
    // GPU profiling queries
    unsigned int queryShadow = 0;
    unsigned int queryGeometry = 0;
    unsigned int queryIBL = 0;
    unsigned int queryWater = 0;
    unsigned int queryPost = 0;

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
    unsigned int waterGridVAO, waterGridVBO, waterGridEBO;
    int waterGridIndexCount;

    void setupGBuffer();
    void setupShadows();
    void setupGeometry();
    
    void renderGeometryPass(Scene* scene, bool useNormalMap, float tessLevel, float explosionFactor);
    void renderLightingPass(Scene* scene);
    void renderForwardPass(Scene* scene, bool useNormalMap, float pSpread, float pSize, float pCount);
    
    void renderEntitiesToGBuffer(Shader* shader, const std::vector<Entity>& entities, bool useNormalMap);
    void renderParticles(Shader* shader, const std::vector<Entity>& entities, float time, float spread, float size, float particleCount);
    
    void renderQuad();
};

#endif
