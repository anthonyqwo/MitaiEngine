#ifndef APPLICATION_H
#define APPLICATION_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

#include "Scene.h"
#include "Model.h"
#include "Renderer.h"

class Application {
public:
    Application();
    ~Application();
    
    bool init();
    void loadDefaultScene();
    void run();

private:
    void processInput();
    void renderImGui();
    void setupResources();

    GLFWwindow* window;
    Scene* scene;
    Renderer* renderer;
    
    unsigned int SCR_WIDTH = 1920;
    unsigned int SCR_HEIGHT = 1080;

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
    
    Model* helmetModel;

    static Application* s_instance;
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
    
    // IBL Helpers
    unsigned int loadCubemap(std::vector<std::string> faces);
};

#endif
