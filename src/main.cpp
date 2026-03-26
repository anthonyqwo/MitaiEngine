#define GLM_FORCE_CTOR_INIT 
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Shader.h"
#include "Camera.h"
#include "Entity.h"
#include "Geometry.h"
#include "Renderer.h"
#include "IBLBaker.h"
#include "Model.h"
#include <iostream>
#include <vector>

unsigned int loadTexture(const char *path);
unsigned int loadCubemap(std::vector<std::string> faces);
unsigned int createSolidTexture(unsigned char r, unsigned char g, unsigned char b);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

std::vector<Entity> sceneEntities;
int selectedEntityIndex = -1;
unsigned int SCR_WIDTH = 1920, SCR_HEIGHT = 1080;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
Camera camera(glm::vec3(0.0f, 2.0f, 8.0f));
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true, cursorDisabled = false;
float deltaTime = 0.0f, lastFrame = 0.0f;

bool useNormalMap = true, light2Moving = true;
float tessLevel = 4.0f, explosionFactor = 0.0f;

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Modern PBR Engine - Stable Version", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    stbi_set_flip_vertically_on_load(true);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 450");

    Shader ourShader("shaders/vertex.glsl", "shaders/fragment.glsl");
    Shader skyboxShader("shaders/skybox_v.glsl", "shaders/skybox_f.glsl");
    Shader shadowShader("shaders/shadow_v.glsl", "shaders/shadow_f.glsl");
    Shader pointShadowShader("shaders/point_shadow_v.glsl", "shaders/point_shadow_f.glsl", "shaders/point_shadow_g.glsl");
    Shader advShader("shaders/adv_v.glsl", "shaders/adv_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl");
    Shader particleShader("shaders/particle_v.glsl", "shaders/particle_f.glsl", "shaders/particle_g.glsl", "shaders/particle_tc.glsl", "shaders/particle_te.glsl");
    Shader advShadowShader("shaders/adv_v.glsl", "shaders/shadow_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl");
    Shader advPointShadowShader("shaders/adv_v.glsl", "shaders/point_shadow_f.glsl", "shaders/adv_point_shadow_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl");

    float pSpread = 2.0f, pSize = 0.4f, pCount = 256.0f;

    // --- 幾何體頂點定義 (Pos, Norm, UV, Tangent) -> Stride 為 11 ---
    float cubeVertices[] = {
        // Position            // Normal           // UV      // Tangent
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,-1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,-1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f,-1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f,-1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,-1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,-1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.0f
    };

    float floorWallV[] = {
        // Floor (y=0) -> Normal (0,1,0), Tangent (1,0,0)
        -7,0,-7, 0,1,0, 0,5, 1,0,0,
         7,0,-7, 0,1,0, 5,5, 1,0,0,
         7,0, 7, 0,1,0, 5,0, 1,0,0,
         7,0, 7, 0,1,0, 5,0, 1,0,0,
        -7,0, 7, 0,1,0, 0,0, 1,0,0,
        -7,0,-7, 0,1,0, 0,5, 1,0,0,
        // Back Wall (z=-7) -> Normal (0,0,1), Tangent (1,0,0)
        -7,0,-7, 0,0,1, 0,0, 1,0,0,
         7,0,-7, 0,0,1, 5,0, 1,0,0,
         7,5,-7, 0,0,1, 5,2, 1,0,0,
         7,5,-7, 0,0,1, 5,2, 1,0,0,
        -7,5,-7, 0,0,1, 0,2, 1,0,0,
        -7,0,-7, 0,0,1, 0,0, 1,0,0,
        // Left Wall (x=-7) -> Normal (1,0,0), Tangent (0,0,-1)
        -7,0, 7, 1,0,0, 0,0, 0,0,-1,
        -7,0,-7, 1,0,0, 5,0, 0,0,-1,
        -7,5,-7, 1,0,0, 5,2, 0,0,-1,
        -7,5,-7, 1,0,0, 5,2, 0,0,-1,
        -7,5, 7, 1,0,0, 0,2, 0,0,-1,
        -7,0, 7, 1,0,0, 0,0, 0,0,-1
    };

    unsigned int cubeVAO, cubeVBO; glGenVertexArrays(1, &cubeVAO); glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO); glBindBuffer(GL_ARRAY_BUFFER, cubeVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    
    unsigned int floorVAO, floorVBO; glGenVertexArrays(1, &floorVAO); glGenBuffers(1, &floorVBO);
    glBindVertexArray(floorVAO); glBindBuffer(GL_ARRAY_BUFFER, floorVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(floorWallV), floorWallV, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));

    unsigned int sphereVAO, sphereVBO; int sphereCount; Geometry::setupSphere(sphereVAO, sphereVBO, sphereCount);
    unsigned int icoVAO, icoVBO; int icoCount; Geometry::setupIcosahedron(icoVAO, icoVBO, icoCount);

    float tetraV[] = { 0,1,0, 0,1,0, -1,-1,1, -1,-1,1, 1,-1,1, 1,-1,1, 0,-1,-1, 0,-1,-1 };
    unsigned int tetraIdx[] = { 0,1,2, 0,2,3, 0,3,1, 1,3,2 };
    unsigned int tVAO, tVBO, tEBO; glGenVertexArrays(1, &tVAO); glGenBuffers(1, &tVBO); glGenBuffers(1, &tEBO);
    glBindVertexArray(tVAO); glBindBuffer(GL_ARRAY_BUFFER, tVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(tetraV), tetraV, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(tetraIdx), tetraIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);

    // --- Skybox 靜態初始化 (移出迴圈) ---
    float skyVertices[] = { -1,1,-1, -1,-1,-1, 1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1, -1,-1,1, -1,-1,-1, -1,1,-1, -1,1,-1, -1,1,1, -1,-1,1, 1,-1,-1, 1,-1,1, 1,1,1, 1,1,1, 1,1,-1, 1,-1,-1, -1,-1,1, -1,1,1, 1,1,1, 1,1,1, 1,-1,1, -1,-1,1, -1,1,-1, 1,1,-1, 1,1,1, 1,1,1, -1,1,1, -1,1,-1, -1,-1,-1, -1,-1,1, 1,-1,-1, 1,-1,-1, -1,-1,1, 1,-1,1 };
    unsigned int skVAO, skVBO; glGenVertexArrays(1, &skVAO); glGenBuffers(1, &skVBO);
    glBindVertexArray(skVAO); glBindBuffer(GL_ARRAY_BUFFER, skVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(skyVertices), skyVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0); glEnableVertexAttribArray(0);

    // --- Shadow Maps Setup ---
    unsigned int depthFBO, depthMap; glGenFramebuffers(1, &depthFBO); glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap); glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float bCol[] = { 1,1,1,1 }; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bCol);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int pointShadowFBO, depthCubemap; glGenFramebuffers(1, &pointShadowFBO); glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    for(int i=0; i<6; i++) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO); glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int texDiff = loadTexture("assets/container.jpg"), texSpec = loadTexture("assets/container_specular.png"), texNorm = loadTexture("assets/container_normal.png");
    unsigned int waterNorm = loadTexture("assets/water_normal.jpg");
    unsigned int floorDiff = loadTexture("assets/wall.jpg"), floorNorm = loadTexture("assets/wall_normal.jpg"), whiteTex = createSolidTexture(255, 255, 255), flatNormalTex = createSolidTexture(128, 128, 255);
    unsigned int skybox = loadCubemap({"assets/skybox/right.jpg","assets/skybox/left.jpg","assets/skybox/top.jpg","assets/skybox/bottom.jpg","assets/skybox/front.jpg","assets/skybox/back.jpg"});

    unsigned int irradianceMap, prefilterMap, brdfLUT;
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    IBLBaker::bake(skybox, cubeVAO, irradianceMap, prefilterMap, brdfLUT);

    Model helmetModel("assets/models/DamagedHelmet.glb");
    Entity helmetEnt("Damaged Helmet", MODEL, glm::vec3(0, 1.0f, 0), glm::vec3(1));
    helmetEnt.model = &helmetModel;
    helmetEnt.scale = glm::vec3(1.0f);
    helmetEnt.metallic = 1.0f; helmetEnt.roughness = 1.0f; helmetEnt.ambient = 1.0f; helmetEnt.reflectivity = 1.0f;
    sceneEntities.push_back(helmetEnt);

    Entity floorEnt("Floor", FLOOR, glm::vec3(0, -0.5f, 0), glm::vec3(0.6f));
    floorEnt.roughness=0.05f; floorEnt.metallic=0.0f; floorEnt.ambient=1.0f; floorEnt.reflectivity=0.05f;
    sceneEntities.push_back(floorEnt);
    Entity cubeEnt("Dynamic Cube", CUBE, glm::vec3(0, 0.5f, 0), glm::vec3(1));
    cubeEnt.roughness=1.0f; cubeEnt.metallic=1.0f; cubeEnt.ambient=1.0f; cubeEnt.reflectivity=0.15f;
    sceneEntities.push_back(cubeEnt);
    Entity sphereEnt("Standard Sphere", SPHERE, glm::vec3(2, 1, -2), glm::vec3(1, 0.5f, 0));
    sphereEnt.roughness=0.1f; sphereEnt.metallic=1.0f; sphereEnt.ambient=1.0f; sphereEnt.reflectivity=0.45f;
    sceneEntities.push_back(sphereEnt);
    Entity icoEnt("Advanced Sphere", ADV_SPHERE, glm::vec3(-2, 0.5f, 2), glm::vec3(1, 0.2f, 0.5f));
    icoEnt.roughness=0.2f; icoEnt.metallic=1.0f; icoEnt.ambient=1.0f; icoEnt.reflectivity=0.3f;
    sceneEntities.push_back(icoEnt);
    Entity waterEntity("Water Surface", WATER, glm::vec3(0, -0.49f, 0), glm::vec3(0.1f, 0.3f, 0.6f));
    waterEntity.roughness=0.1f; waterEntity.reflectivity=0.4f; waterEntity.metallic=0.0f;
    sceneEntities.push_back(waterEntity);

    Entity sunEnt("Main Sun", CUBE, glm::vec3(5.0f, 10.0f, 5.0f), glm::vec3(1.0f, 0.95f, 0.8f));
    sunEnt.isLight=true; sunEnt.lightColor=glm::vec3(1.0f, 0.95f, 0.8f); sunEnt.lightIntensity=2.0f; sunEnt.scale=glm::vec3(0.2f);
    sceneEntities.push_back(sunEnt);
    Entity lampEnt("Point Light", CUBE, glm::vec3(-2.0f, 2.0f, 1.0f), glm::vec3(1.0f, 0.5f, 0.0f));
    lampEnt.isLight=true; lampEnt.lightColor=glm::vec3(1.0f, 0.5f, 0.0f); lampEnt.lightIntensity=2.0f; lampEnt.scale=glm::vec3(0.2f);
    sceneEntities.push_back(lampEnt);
    sceneEntities.push_back(Entity("Particle Source", PARTICLE, glm::vec3(0, 1.0f, 0), glm::vec3(124.0f/255.0f, 117.0f/255.0f, 112.0f/255.0f)));

    // --- 移除已在 Shader 中綁定的重複 setInt ---
    skyboxShader.use(); skyboxShader.setInt("skybox", 0);

    glEnable(GL_DEPTH_TEST); 
    while (!glfwWindowShouldClose(window)) {
        float cur = (float)glfwGetTime(); deltaTime = cur - lastFrame; lastFrame = cur;
        processInput(window);
        for(auto& e : sceneEntities){
            if(e.name=="Dynamic Cube") e.rotation.y+=30*deltaTime;
            if(e.name=="Icosahedron") e.rotation.y+=20*deltaTime;
            if(light2Moving && e.name=="Point Light") e.position=glm::vec3(sin(cur)*3, 2, cos(cur)*3);
        }
        glm::vec3 sP(5,10,5), pP(-2,2,1);
        for(const auto& e : sceneEntities){ if(e.name=="Main Sun") sP=e.position; if(e.name=="Point Light") pP=e.position; }

        // 1. Sun Shadow Pass
        glm::mat4 lProj=glm::ortho(-10.0f,10.0f,-10.0f,10.0f,1.0f,30.0f), lView=glm::lookAt(sP,glm::vec3(0),glm::vec3(0,1,0)), lSpace=lProj*lView;
        glViewport(0,0,SHADOW_WIDTH,SHADOW_HEIGHT); glBindFramebuffer(GL_FRAMEBUFFER, depthFBO); glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
        
        // Pass A: 普通物件陰影
        shadowShader.use(); shadowShader.setMat4("lightSpaceMatrix", lSpace);
        Renderer::renderEntities(shadowShader, sceneEntities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
        
        // Pass B: 進階物件陰影 (需同步位移)
        advShadowShader.use(); advShadowShader.setMat4("lightSpaceMatrix", lSpace);
        advShadowShader.setFloat("tessLevel", tessLevel); advShadowShader.setFloat("explosionFactor", explosionFactor);
        Renderer::renderEntities(advShadowShader, sceneEntities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
        
        glCullFace(GL_BACK); glDisable(GL_CULL_FACE); glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. Point Light Shadow Pass
        float far_p=25.0f; glm::mat4 pProj=glm::perspective(glm::radians(90.0f),1.0f,1.0f,far_p);
        std::vector<glm::mat4> pMats;
        pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(1,0,0),glm::vec3(0,-1,0))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(-1,0,0),glm::vec3(0,-1,0)));
        pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,1,0),glm::vec3(0,0,1))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,-1,0),glm::vec3(0,0,-1)));
        pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,0,1),glm::vec3(0,-1,0))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,0,-1),glm::vec3(0,-1,0)));
        glViewport(0,0,1024,1024); glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO); glClear(GL_DEPTH_BUFFER_BIT);

        // Pass A: 普通物件點陰影
        pointShadowShader.use(); for(int i=0;i<6;i++) pointShadowShader.setMat4("shadowMatrices["+std::to_string(i)+"]", pMats[i]);
        pointShadowShader.setFloat("far_plane", far_p); pointShadowShader.setVec3("lightPos", pP);
        Renderer::renderEntities(pointShadowShader, sceneEntities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
        
        // Pass B: 進階物件點陰影
        advPointShadowShader.use(); for(int i=0;i<6;i++) advPointShadowShader.setMat4("shadowMatrices["+std::to_string(i)+"]", pMats[i]);
        advPointShadowShader.setFloat("far_plane", far_p); advPointShadowShader.setVec3("lightPos", pP);
        advPointShadowShader.setFloat("tessLevel", tessLevel); advPointShadowShader.setFloat("explosionFactor", explosionFactor);
        Renderer::renderEntities(advPointShadowShader, sceneEntities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 3. Main Pass
        glViewport(0,0,SCR_WIDTH,SCR_HEIGHT); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        ourShader.use(); ourShader.setMat4("projection", glm::perspective(glm::radians(camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
        ourShader.setMat4("view", camera.GetViewMatrix()); ourShader.setMat4("lightSpaceMatrix", lSpace); 
        ourShader.setVec3("viewPos", camera.Position); ourShader.setFloat("far_plane", far_p);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthMap);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, brdfLUT);
        glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

        // 3.1 繪製天空盒 (作為背景，必須在透明物件之前)
        glDepthFunc(GL_LEQUAL); skyboxShader.use();
        skyboxShader.setMat4("view", glm::mat4(glm::mat3(camera.GetViewMatrix()))); skyboxShader.setMat4("projection", glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH/SCR_HEIGHT, 0.1f, 100.0f));
        glBindVertexArray(skVAO); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, skybox); glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        // 3.2 繪製場景實體 (包含透明水體)
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Pass A: 標準 PBR 物件
        Renderer::renderEntities(ourShader, sceneEntities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, texDiff, texSpec, texNorm, floorDiff, floorNorm, waterNorm, whiteTex, flatNormalTex, useNormalMap, camera.Position);
        
        // Pass B: 進階細分 PBR 物件
        advShader.use();
        advShader.setMat4("projection", glm::perspective(glm::radians(camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
        advShader.setMat4("view", camera.GetViewMatrix());
        advShader.setMat4("lightSpaceMatrix", lSpace);
        advShader.setVec3("viewPos", camera.Position);
        advShader.setFloat("far_plane", far_p);
        advShader.setFloat("tessLevel", tessLevel);
        advShader.setFloat("explosionFactor", explosionFactor);
        
        // --- 補齊 IBL 資源綁定 ---
        advShader.setInt("shadowMap", 3);
        advShader.setInt("skybox", 4);
        advShader.setInt("irradianceMap", 5);
        advShader.setInt("prefilterMap", 6);
        advShader.setInt("brdfLUT", 7);
        advShader.setInt("pointShadowMap", 8);
        
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthMap);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, brdfLUT);
        glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

        Renderer::renderEntities(advShader, sceneEntities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, texDiff, texSpec, texNorm, floorDiff, floorNorm, waterNorm, whiteTex, flatNormalTex, useNormalMap, camera.Position);
        
        // 3.3 繪製粒子與進階效果
        particleShader.use();
        particleShader.setMat4("projection", glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f));
        particleShader.setMat4("view", camera.GetViewMatrix());
        Renderer::renderParticles(particleShader, sceneEntities, (float)glfwGetTime(), pSpread, pSize, pCount);

        glDisable(GL_BLEND);

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        { ImGui::Begin("Scene Hierarchy");
          if (ImGui::Button("Add Cube")) sceneEntities.push_back(Entity("New Cube", CUBE, camera.Position + camera.Front*2.0f));
          ImGui::SameLine(); if (ImGui::Button("Add Sphere")) sceneEntities.push_back(Entity("New Sphere", SPHERE, camera.Position + camera.Front*2.0f));
          ImGui::SameLine(); if (ImGui::Button("Add Adv Sphere")) sceneEntities.push_back(Entity("Adv Sphere", ADV_SPHERE, camera.Position + camera.Front*2.0f, glm::vec3(0.8,0.2,0.5)));
          ImGui::SameLine(); if (ImGui::Button("Add Water")) { Entity w("Water", WATER, camera.Position + camera.Front*5.0f, glm::vec3(0.1,0.4,0.8)); w.roughness=0.05f; w.reflectivity=0.4f; sceneEntities.push_back(w); }
          ImGui::SameLine(); if (ImGui::Button("Add Sun")) { Entity s("Sun", CUBE, camera.Position + camera.Front*5.0f, glm::vec3(1,0.95,0.8)); s.isLight=true; s.lightColor=glm::vec3(1,0.95,0.8); s.lightIntensity=2.0f; s.scale=glm::vec3(0.2f); sceneEntities.push_back(s); }
          ImGui::SameLine(); if (ImGui::Button("Add Light")) { Entity p("Point Light", CUBE, camera.Position + camera.Front*3.0f, glm::vec3(1,0.5,0)); p.isLight=true; p.lightColor=glm::vec3(1,0.5,0); p.lightIntensity=2.0f; p.scale=glm::vec3(0.2f); sceneEntities.push_back(p); }
          ImGui::Separator();
          for (int i=0; i<sceneEntities.size(); i++) if (ImGui::Selectable((sceneEntities[i].name + "##" + std::to_string(i)).c_str(), selectedEntityIndex == i)) selectedEntityIndex = i;
          ImGui::End(); }
        { ImGui::Begin("Inspector");
          if (selectedEntityIndex >= 0 && selectedEntityIndex < sceneEntities.size()) {
              Entity& e = sceneEntities[selectedEntityIndex];
              ImGui::Checkbox("Visible", &e.visible);
              ImGui::SliderFloat3("Position", glm::value_ptr(e.position), -15.0f, 15.0f);
              if (!e.isLight) {
                  ImGui::SliderFloat3("Rotation", glm::value_ptr(e.rotation), 0.0f, 360.0f);
                  ImGui::SliderFloat3("Scale", glm::value_ptr(e.scale), 0.1f, 15.0f);
                  ImGui::ColorEdit3("Col", glm::value_ptr(e.color));
                  ImGui::Separator(); ImGui::Text("PBR Material");
                  ImGui::SliderFloat("Roughness", &e.roughness, 0.05f, 1.0f);
                  ImGui::SliderFloat("Metallic", &e.metallic, 0.0f, 1.0f);
                  ImGui::SliderFloat("Ambient (AO)", &e.ambient, 0.0f, 1.0f);
                  ImGui::SliderFloat("Reflectivity", &e.reflectivity, 0.0f, 1.0f);
              } else {
                  ImGui::ColorEdit3("Light Color", glm::value_ptr(e.lightColor));
                  ImGui::SliderFloat("Intensity", &e.lightIntensity, 0.0f, 100.0f);
              }
              ImGui::Separator(); ImGui::Text("Effects");
              ImGui::Checkbox("Dynamic Texture", &e.dynamicTexture);
              if (e.dynamicTexture) ImGui::SliderFloat("Tex Speed", &e.texSpeed, -1.0f, 1.0f);
              if (ImGui::Button("Delete")) { sceneEntities.erase(sceneEntities.begin()+selectedEntityIndex); selectedEntityIndex = -1; }
          } else { ImGui::Text("Select an entity"); }
          ImGui::End(); }
        { ImGui::Begin("Engine Controls"); 
          ImGui::Checkbox("Normal Map", &useNormalMap); ImGui::Checkbox("Light 2 Moving", &light2Moving); ImGui::Separator();
          ImGui::Text("Advanced Global"); ImGui::SliderFloat("Tess Level", &tessLevel, 1, 64); ImGui::SliderFloat("Explosion", &explosionFactor, 0, 1);
          ImGui::Separator();
          ImGui::Text("Particle System"); ImGui::SliderFloat("P Count", &pCount, 1, 256); ImGui::SliderFloat("P Spread", &pSpread, 0.1f, 5.0f); ImGui::SliderFloat("P Size", &pSize, 0.01f, 0.5f);
          ImGui::End(); }

        ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window); glfwPollEvents();
    }
    glfwTerminate(); return 0;
}

unsigned int loadTexture(const char *path) {
    unsigned int tid; glGenTextures(1, &tid); int w,h,c; unsigned char *d = stbi_load(path, &w, &h, &c, 0);
    if(d){ GLenum f=(c==1)?GL_RED:(c==3?GL_RGB:GL_RGBA); glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, d); glGenerateMipmap(GL_TEXTURE_2D); }
    else { unsigned char fb[]={180,180,180}; glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, fb); }
    stbi_image_free(d); return tid;
}
unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int tid; glGenTextures(1, &tid); glBindTexture(GL_TEXTURE_CUBE_MAP, tid); stbi_set_flip_vertically_on_load(false);
    for(int i=0; i<6; i++){ int w,h,c; unsigned char *d=stbi_load(faces[i].c_str(), &w, &h, &c, 0); if(d) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, d); stbi_image_free(d); }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    stbi_set_flip_vertically_on_load(true); return tid;
}
unsigned int createSolidTexture(unsigned char r, unsigned char g, unsigned char b) {
    unsigned int tid; glGenTextures(1, &tid); unsigned char d[]={r,g,b}; glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, d); return tid;
}
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    static bool tabP = false; if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !tabP) { cursorDisabled = !cursorDisabled; glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL); tabP = true; } else if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE) tabP = false;
    if (cursorDisabled) { 
        bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime, sprint); 
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime, sprint); 
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime, sprint); 
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime, sprint); 
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.ProcessKeyboard(UP, deltaTime, sprint);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera.ProcessKeyboard(DOWN, deltaTime, sprint);
    }
}
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!cursorDisabled) return; float xpos = (float)xposIn, ypos = (float)yposIn; if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoff = xpos - lastX, yoff = lastY - ypos; lastX = xpos; lastY = ypos; camera.ProcessMouseMovement(xoff, yoff);
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }
