#ifndef APPLICATION_H
#define APPLICATION_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

#include "Scene.h"
#include "Shader.h"
#include "Model.h"

class Application {
public:
    Application();
    ~Application();
    
    bool init();
    void loadDefaultScene();
    void run();

private:
    void processInput();
    void render(glm::mat4 lSpace, float far_p, std::vector<glm::mat4> pMats);
    void renderImGui();
    void setupResources();

    GLFWwindow* window;
    Scene* scene;
    
    unsigned int SCR_WIDTH = 1920;
    unsigned int SCR_HEIGHT = 1080;
    unsigned int SHADOW_WIDTH = 2048;
    unsigned int SHADOW_HEIGHT = 2048;

    float deltaTime;
    float lastFrame;
    bool cursorDisabled;
    float lastX, lastY;
    bool firstMouse;

    bool useNormalMap;
    bool light2Moving;
    float tessLevel;
    float explosionFactor;
    float pSpread, pSize, pCount;
    int selectedEntityIndex;
    
    Shader* ourShader;
    Shader* skyboxShader;
    Shader* shadowShader;
    Shader* pointShadowShader;
    Shader* advShader;
    Shader* particleShader;
    Shader* advShadowShader;
    Shader* advPointShadowShader;
    
    Model* helmetModel;

    unsigned int cubeVAO, cubeVBO;
    unsigned int floorVAO, floorVBO;
    unsigned int sphereVAO, sphereVBO;
    int sphereCount;
    unsigned int icoVAO, icoVBO;
    int icoCount;
    unsigned int skVAO, skVBO;
    
    unsigned int depthFBO, depthMap;
    unsigned int pointShadowFBO, depthCubemap;
    
    unsigned int texDiff, texSpec, texNorm, waterNorm, floorDiff, floorNorm, whiteTex, flatNormalTex;

    static Application* s_instance;
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
    
    unsigned int loadTexture(const char *path);
    unsigned int loadCubemap(std::vector<std::string> faces);
    unsigned int createSolidTexture(unsigned char r, unsigned char g, unsigned char b);
};

#endif
