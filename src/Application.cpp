#include "Application.h"
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <iostream>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <GLFW/glfw3native.h>
#include <imm.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Renderer.h"
#include "Geometry.h"
#include "IBLBaker.h"

Application* Application::s_instance = nullptr;

Application::Application() {
    s_instance = this;
    window = nullptr;
    scene = new Scene();
    
    deltaTime = 0.0f;
    lastFrame = 0.0f;
    cursorDisabled = false;
    lastX = SCR_WIDTH / 2.0f; 
    lastY = SCR_HEIGHT / 2.0f;
    firstMouse = true;

    useNormalMap = true;
    light2Moving = true;
    tessLevel = 4.0f;
    explosionFactor = 0.0f;
    pSpread = 2.0f; 
    pSize = 0.4f; 
    pCount = 256.0f;
    selectedEntityIndex = -1;
}

Application::~Application() {
    delete scene;
    delete ourShader;
    delete skyboxShader;
    delete shadowShader;
    delete pointShadowShader;
    delete advShader;
    delete particleShader;
    delete advShadowShader;
    delete advPointShadowShader;
    delete helmetModel;
}

bool Application::init() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "PBR Engine V4.5 - Refactored", NULL, NULL);
    if (!window) return false;

    glfwMakeContextCurrent(window);
    
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    ImmAssociateContext(hwnd, NULL);
#endif

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    stbi_set_flip_vertically_on_load(true);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 450");
    
    setupResources();
    return true;
}

void Application::setupResources() {
    ourShader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
    skyboxShader = new Shader("shaders/skybox_v.glsl", "shaders/skybox_f.glsl");
    shadowShader = new Shader("shaders/shadow_v.glsl", "shaders/shadow_f.glsl");
    pointShadowShader = new Shader("shaders/point_shadow_v.glsl", "shaders/point_shadow_f.glsl", "shaders/point_shadow_g.glsl");
    advShader = new Shader("shaders/adv_v.glsl", "shaders/adv_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl");
    particleShader = new Shader("shaders/particle_v.glsl", "shaders/particle_f.glsl", "shaders/particle_g.glsl", "shaders/particle_tc.glsl", "shaders/particle_te.glsl");
    advShadowShader = new Shader("shaders/adv_v.glsl", "shaders/shadow_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl");
    advPointShadowShader = new Shader("shaders/adv_v.glsl", "shaders/point_shadow_f.glsl", "shaders/adv_point_shadow_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl");

    float cubeVertices[] = {
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
        -7,0,-7, 0,1,0, 0,5, 1,0,0,  7,0,-7, 0,1,0, 5,5, 1,0,0,  7,0, 7, 0,1,0, 5,0, 1,0,0,
         7,0, 7, 0,1,0, 5,0, 1,0,0, -7,0, 7, 0,1,0, 0,0, 1,0,0, -7,0,-7, 0,1,0, 0,5, 1,0,0,
        -7,0,-7, 0,0,1, 0,0, 1,0,0,  7,0,-7, 0,0,1, 5,0, 1,0,0,  7,5,-7, 0,0,1, 5,2, 1,0,0,
         7,5,-7, 0,0,1, 5,2, 1,0,0, -7,5,-7, 0,0,1, 0,2, 1,0,0, -7,0,-7, 0,0,1, 0,0, 1,0,0,
        -7,0, 7, 1,0,0, 0,0, 0,0,-1, -7,0,-7, 1,0,0, 5,0, 0,0,-1, -7,5,-7, 1,0,0, 5,2, 0,0,-1,
        -7,5,-7, 1,0,0, 5,2, 0,0,-1, -7,5, 7, 1,0,0, 0,2, 0,0,-1, -7,0, 7, 1,0,0, 0,0, 0,0,-1
    };

    glGenVertexArrays(1, &cubeVAO); glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO); glBindBuffer(GL_ARRAY_BUFFER, cubeVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    
    glGenVertexArrays(1, &floorVAO); glGenBuffers(1, &floorVBO);
    glBindVertexArray(floorVAO); glBindBuffer(GL_ARRAY_BUFFER, floorVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(floorWallV), floorWallV, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));

    Geometry::setupSphere(sphereVAO, sphereVBO, sphereCount);
    Geometry::setupIcosahedron(icoVAO, icoVBO, icoCount);

    float skyVertices[] = { -1,1,-1, -1,-1,-1, 1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1, -1,-1,1, -1,-1,-1, -1,1,-1, -1,1,-1, -1,1,1, -1,-1,1, 1,-1,-1, 1,-1,1, 1,1,1, 1,1,1, 1,1,-1, 1,-1,-1, -1,-1,1, -1,1,1, 1,1,1, 1,1,1, 1,-1,1, -1,-1,1, -1,1,-1, 1,1,-1, 1,1,1, 1,1,1, -1,1,1, -1,1,-1, -1,-1,-1, -1,-1,1, 1,-1,-1, 1,-1,-1, -1,-1,1, 1,-1,1 };
    glGenVertexArrays(1, &skVAO); glGenBuffers(1, &skVBO);
    glBindVertexArray(skVAO); glBindBuffer(GL_ARRAY_BUFFER, skVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(skyVertices), skyVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0); glEnableVertexAttribArray(0);

    glGenFramebuffers(1, &depthFBO); glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap); glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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

    texDiff = loadTexture("assets/container.jpg"), texSpec = loadTexture("assets/container_specular.png"), texNorm = loadTexture("assets/container_normal.png");
    waterNorm = loadTexture("assets/water_normal.jpg");
    floorDiff = loadTexture("assets/wall.jpg"), floorNorm = loadTexture("assets/wall_normal.jpg"), whiteTex = createSolidTexture(255, 255, 255), flatNormalTex = createSolidTexture(128, 128, 255);
    scene->skybox = loadCubemap({"assets/skybox/right.jpg","assets/skybox/left.jpg","assets/skybox/top.jpg","assets/skybox/bottom.jpg","assets/skybox/front.jpg","assets/skybox/back.jpg"});

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    IBLBaker::bake(scene->skybox, cubeVAO, scene->irradianceMap, scene->prefilterMap, scene->brdfLUT);
    
    helmetModel = new Model("assets/models/DamagedHelmet.glb");
    
    skyboxShader->use(); skyboxShader->setInt("skybox", 0);
}

void Application::loadDefaultScene() {
    Entity helmetEnt("Damaged Helmet", MODEL, glm::vec3(0, 1.0f, 0), glm::vec3(1));
    helmetEnt.model = helmetModel;
    helmetEnt.scale = glm::vec3(1.0f);
    helmetEnt.metallic = 1.0f; helmetEnt.roughness = 1.0f; helmetEnt.ambient = 1.0f; helmetEnt.reflectivity = 1.0f;
    helmetEnt.localBounds = helmetModel->localBounds;
    scene->addEntity(helmetEnt);

    Entity floorEnt("Floor", FLOOR, glm::vec3(0, -0.5f, 0), glm::vec3(0.6f));
    floorEnt.roughness=0.05f; floorEnt.metallic=0.0f; floorEnt.ambient=1.0f; floorEnt.reflectivity=0.05f;
    floorEnt.localBounds = AABB(glm::vec3(-7, 0, -7), glm::vec3(7, 5, 7));
    floorEnt.hasCollision = false; 
    scene->addEntity(floorEnt);
    
    Entity cubeEnt("Dynamic Cube", CUBE, glm::vec3(0, 0.5f, 0), glm::vec3(1));
    cubeEnt.roughness=1.0f; cubeEnt.metallic=1.0f; cubeEnt.ambient=1.0f; cubeEnt.reflectivity=0.15f;
    cubeEnt.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    scene->addEntity(cubeEnt);
    
    Entity sphereEnt("Standard Sphere", SPHERE, glm::vec3(2, 1, -2), glm::vec3(1, 0.5f, 0));
    sphereEnt.roughness=0.1f; sphereEnt.metallic=1.0f; sphereEnt.ambient=1.0f; sphereEnt.reflectivity=0.45f;
    sphereEnt.localBounds = AABB(glm::vec3(-1.0f), glm::vec3(1.0f));
    scene->addEntity(sphereEnt);
    
    Entity icoEnt("Advanced Sphere", ADV_SPHERE, glm::vec3(-2, 0.5f, 2), glm::vec3(1, 0.2f, 0.5f));
    icoEnt.roughness=0.2f; icoEnt.metallic=1.0f; icoEnt.ambient=1.0f; icoEnt.reflectivity=0.3f;
    icoEnt.localBounds = AABB(glm::vec3(-1.0f), glm::vec3(1.0f));
    scene->addEntity(icoEnt);
    
    Entity waterEntity("Water Surface", WATER, glm::vec3(0, -0.49f, 0), glm::vec3(0.1f, 0.3f, 0.6f));
    waterEntity.roughness=0.1f; waterEntity.reflectivity=0.4f; waterEntity.metallic=0.0f;
    waterEntity.hasCollision = false;
    scene->addEntity(waterEntity);

    Entity sunEnt("Main Sun", CUBE, glm::vec3(5.0f, 10.0f, 5.0f), glm::vec3(1.0f, 0.95f, 0.8f));
    sunEnt.isLight=true; sunEnt.lightColor=glm::vec3(1.0f, 0.95f, 0.8f); sunEnt.lightIntensity=2.0f; sunEnt.scale=glm::vec3(0.2f);
    sunEnt.hasCollision = false;
    scene->addEntity(sunEnt);
    
    Entity lampEnt("Point Light", CUBE, glm::vec3(-2.0f, 2.0f, 1.0f), glm::vec3(1.0f, 0.5f, 0.0f));
    lampEnt.isLight=true; lampEnt.lightColor=glm::vec3(1.0f, 0.5f, 0.0f); lampEnt.lightIntensity=2.0f; lampEnt.scale=glm::vec3(0.2f);
    lampEnt.hasCollision = false;
    scene->addEntity(lampEnt);
    
    Entity partEnt("Particle Source", PARTICLE, glm::vec3(0, 1.0f, 0), glm::vec3(124.0f/255.0f, 117.0f/255.0f, 112.0f/255.0f));
    partEnt.hasCollision = false;
    scene->addEntity(partEnt);
}

void Application::processInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    
    static bool tabP = false; 
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !tabP) { 
        cursorDisabled = !cursorDisabled; 
        glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL); 
        tabP = true; 
    } else if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE) {
        tabP = false;
    }
    
    if (cursorDisabled) { 
        bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        
        glm::vec3 movement = glm::vec3(0.0f);
        float velocity = scene->camera.MovementSpeed * deltaTime * (sprint ? 2.5f : 1.0f);
        glm::vec3 horizontalFront = glm::normalize(glm::vec3(scene->camera.Front.x, 0.0f, scene->camera.Front.z));
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement += horizontalFront * velocity; 
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= horizontalFront * velocity; 
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= scene->camera.Right * velocity; 
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += scene->camera.Right * velocity; 
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) movement += scene->camera.WorldUp * velocity;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) movement -= scene->camera.WorldUp * velocity;
        
        scene->processCollisions(movement);
    }
}

void Application::render(glm::mat4 lSpace, float far_p, std::vector<glm::mat4> pMats) {
    // 1. Sun Shadow Pass
    glViewport(0,0,SHADOW_WIDTH,SHADOW_HEIGHT); glBindFramebuffer(GL_FRAMEBUFFER, depthFBO); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
    shadowShader->use(); shadowShader->setMat4("lightSpaceMatrix", lSpace);
    Renderer::renderEntities(*shadowShader, scene->entities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
    advShadowShader->use(); advShadowShader->setMat4("lightSpaceMatrix", lSpace);
    advShadowShader->setFloat("tessLevel", tessLevel); advShadowShader->setFloat("explosionFactor", explosionFactor);
    Renderer::renderEntities(*advShadowShader, scene->entities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
    glCullFace(GL_BACK); glDisable(GL_CULL_FACE); glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. Point Shadow Pass
    glm::vec3 pP(-2,2,1); // Find actual point light pos
    for(const auto& e : scene->entities){ if(e.name=="Point Light") pP=e.position; }

    glViewport(0,0,1024,1024); glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO); glClear(GL_DEPTH_BUFFER_BIT);
    pointShadowShader->use(); for(int i=0;i<6;i++) pointShadowShader->setMat4("shadowMatrices["+std::to_string(i)+"]", pMats[i]);
    pointShadowShader->setFloat("far_plane", far_p); pointShadowShader->setVec3("lightPos", pP);
    Renderer::renderEntities(*pointShadowShader, scene->entities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
    advPointShadowShader->use(); for(int i=0;i<6;i++) advPointShadowShader->setMat4("shadowMatrices["+std::to_string(i)+"]", pMats[i]);
    advPointShadowShader->setFloat("far_plane", far_p); advPointShadowShader->setVec3("lightPos", pP);
    advPointShadowShader->setFloat("tessLevel", tessLevel); advPointShadowShader->setFloat("explosionFactor", explosionFactor);
    Renderer::renderEntities(*advPointShadowShader, scene->entities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, 0,0,0,0,0,0, whiteTex, flatNormalTex, useNormalMap, glm::vec3(0));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 3. Main Pass
    glViewport(0,0,SCR_WIDTH,SCR_HEIGHT); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    ourShader->use(); ourShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
    ourShader->setMat4("view", scene->camera.GetViewMatrix()); ourShader->setMat4("lightSpaceMatrix", lSpace); 
    ourShader->setVec3("viewPos", scene->camera.Position); ourShader->setFloat("far_plane", far_p);
    
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthMap);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP, scene->skybox);
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, scene->irradianceMap);
    glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, scene->prefilterMap);
    glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, scene->brdfLUT);
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

    // Skybox
    glDepthFunc(GL_LEQUAL); skyboxShader->use();
    skyboxShader->setMat4("view", glm::mat4(glm::mat3(scene->camera.GetViewMatrix()))); skyboxShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom), (float)SCR_WIDTH/SCR_HEIGHT, 0.1f, 100.0f));
    glBindVertexArray(skVAO); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, scene->skybox); glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    Renderer::renderEntities(*ourShader, scene->entities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, texDiff, texSpec, texNorm, floorDiff, floorNorm, waterNorm, whiteTex, flatNormalTex, useNormalMap, scene->camera.Position);
    
    advShader->use();
    advShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f));
    advShader->setMat4("view", scene->camera.GetViewMatrix());
    advShader->setMat4("lightSpaceMatrix", lSpace);
    advShader->setVec3("viewPos", scene->camera.Position);
    advShader->setFloat("far_plane", far_p);
    advShader->setFloat("tessLevel", tessLevel);
    advShader->setFloat("explosionFactor", explosionFactor);
    
    advShader->setInt("shadowMap", 3);
    advShader->setInt("skybox", 4);
    advShader->setInt("irradianceMap", 5);
    advShader->setInt("prefilterMap", 6);
    advShader->setInt("brdfLUT", 7);
    advShader->setInt("pointShadowMap", 8);
    
    Renderer::renderEntities(*advShader, scene->entities, cubeVAO, floorVAO, sphereVAO, sphereCount, icoVAO, icoCount, texDiff, texSpec, texNorm, floorDiff, floorNorm, waterNorm, whiteTex, flatNormalTex, useNormalMap, scene->camera.Position);
    
    particleShader->use();
    particleShader->setMat4("projection", glm::perspective(glm::radians(scene->camera.Zoom), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f));
    particleShader->setMat4("view", scene->camera.GetViewMatrix());
    Renderer::renderParticles(*particleShader, scene->entities, (float)glfwGetTime(), pSpread, pSize, pCount);

    glDisable(GL_BLEND);
}

void Application::renderImGui() {
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
    { ImGui::Begin("Scene Hierarchy");
      if (ImGui::Button("Add Cube")) { Entity e("New Cube", CUBE, scene->camera.Position + scene->camera.Front*2.0f); e.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); scene->addEntity(e); }
      ImGui::SameLine(); if (ImGui::Button("Add Sphere")) { Entity e("New Sphere", SPHERE, scene->camera.Position + scene->camera.Front*2.0f); e.localBounds = AABB(glm::vec3(-1), glm::vec3(1)); scene->addEntity(e); }
      ImGui::SameLine(); if (ImGui::Button("Add Adv Sphere")) { Entity e("Adv Sphere", ADV_SPHERE, scene->camera.Position + scene->camera.Front*2.0f, glm::vec3(0.8,0.2,0.5)); e.localBounds = AABB(glm::vec3(-1), glm::vec3(1)); scene->addEntity(e); }
      ImGui::SameLine(); if (ImGui::Button("Add Water")) { Entity w("Water", WATER, scene->camera.Position + scene->camera.Front*5.0f, glm::vec3(0.1,0.4,0.8)); w.roughness=0.05f; w.reflectivity=0.4f; w.hasCollision=false; scene->addEntity(w); }
      ImGui::SameLine(); if (ImGui::Button("Add Sun")) { Entity s("Sun", CUBE, scene->camera.Position + scene->camera.Front*5.0f, glm::vec3(1,0.95,0.8)); s.isLight=true; s.lightColor=glm::vec3(1,0.95,0.8); s.lightIntensity=2.0f; s.scale=glm::vec3(0.2f); s.hasCollision=false; scene->addEntity(s); }
      ImGui::SameLine(); if (ImGui::Button("Add Light")) { Entity p("Point Light", CUBE, scene->camera.Position + scene->camera.Front*3.0f, glm::vec3(1,0.5,0)); p.isLight=true; p.lightColor=glm::vec3(1,0.5,0); p.lightIntensity=2.0f; p.scale=glm::vec3(0.2f); p.hasCollision=false; scene->addEntity(p); }
      ImGui::Separator();
      for (int i=0; i<scene->entities.size(); i++) if (ImGui::Selectable((scene->entities[i].name + "##" + std::to_string(i)).c_str(), selectedEntityIndex == i)) selectedEntityIndex = i;
      ImGui::End(); }
    { ImGui::Begin("Inspector");
      if (selectedEntityIndex >= 0 && selectedEntityIndex < scene->entities.size()) {
          Entity& e = scene->entities[selectedEntityIndex];
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
          if (ImGui::Button("Delete")) { scene->entities.erase(scene->entities.begin()+selectedEntityIndex); selectedEntityIndex = -1; }
      } else { ImGui::Text("Select an entity"); }
      ImGui::End(); }
    { ImGui::Begin("Engine Controls"); 
      ImGui::Checkbox("Normal Map", &useNormalMap); ImGui::Checkbox("Light 2 Moving", &light2Moving); ImGui::Separator();
      ImGui::Text("Advanced Global"); ImGui::SliderFloat("Tess Level", &tessLevel, 1, 64); ImGui::SliderFloat("Explosion", &explosionFactor, 0, 1);
      ImGui::Separator();
      ImGui::Text("Particle System"); ImGui::SliderFloat("P Count", &pCount, 1, 256); ImGui::SliderFloat("P Spread", &pSpread, 0.1f, 5.0f); ImGui::SliderFloat("P Size", &pSize, 0.01f, 0.5f);
      ImGui::End(); }

    ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::run() {
    glEnable(GL_DEPTH_TEST); 
    while (!glfwWindowShouldClose(window)) {
        float cur = (float)glfwGetTime(); 
        deltaTime = cur - lastFrame; 
        lastFrame = cur;
        
        processInput();
        scene->update(deltaTime, cur, light2Moving);

        glm::vec3 sP(5,10,5), pP(-2,2,1);
        for(const auto& e : scene->entities){ if(e.name=="Main Sun") sP=e.position; if(e.name=="Point Light") pP=e.position; }

        glm::mat4 lProj=glm::ortho(-10.0f,10.0f,-10.0f,10.0f,1.0f,30.0f), lView=glm::lookAt(sP,glm::vec3(0),glm::vec3(0,1,0)), lSpace=lProj*lView;
        float far_p=25.0f; glm::mat4 pProj=glm::perspective(glm::radians(90.0f),1.0f,1.0f,far_p);
        std::vector<glm::mat4> pMats;
        pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(1,0,0),glm::vec3(0,-1,0))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(-1,0,0),glm::vec3(0,-1,0)));
        pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,1,0),glm::vec3(0,0,1))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,-1,0),glm::vec3(0,0,-1)));
        pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,0,1),glm::vec3(0,-1,0))); pMats.push_back(pProj*glm::lookAt(pP,pP+glm::vec3(0,0,-1),glm::vec3(0,-1,0)));

        render(lSpace, far_p, pMats);
        renderImGui();
        
        glfwSwapBuffers(window); glfwPollEvents();
    }
}

void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height) { 
    glViewport(0, 0, width, height); 
}

void Application::mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!s_instance->cursorDisabled) return; 
    float xpos = (float)xposIn, ypos = (float)yposIn; 
    if (s_instance->firstMouse) { s_instance->lastX = xpos; s_instance->lastY = ypos; s_instance->firstMouse = false; }
    float xoff = xpos - s_instance->lastX, yoff = s_instance->lastY - ypos; 
    s_instance->lastX = xpos; s_instance->lastY = ypos; 
    s_instance->scene->camera.ProcessMouseMovement(xoff, yoff);
}

unsigned int Application::loadTexture(const char *path) {
    unsigned int tid; glGenTextures(1, &tid); int w,h,c; unsigned char *d = stbi_load(path, &w, &h, &c, 0);
    if(d){ GLenum f=(c==1)?GL_RED:(c==3?GL_RGB:GL_RGBA); glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, d); glGenerateMipmap(GL_TEXTURE_2D); }
    else { unsigned char fb[]={180,180,180}; glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, fb); }
    stbi_image_free(d); return tid;
}

unsigned int Application::loadCubemap(std::vector<std::string> faces) {
    unsigned int tid; glGenTextures(1, &tid); glBindTexture(GL_TEXTURE_CUBE_MAP, tid); stbi_set_flip_vertically_on_load(false);
    for(int i=0; i<6; i++){ int w,h,c; unsigned char *d=stbi_load(faces[i].c_str(), &w, &h, &c, 0); if(d) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, d); stbi_image_free(d); }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    stbi_set_flip_vertically_on_load(true); return tid;
}

unsigned int Application::createSolidTexture(unsigned char r, unsigned char g, unsigned char b) {
    unsigned int tid; glGenTextures(1, &tid); unsigned char d[]={r,g,b}; glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, d); return tid;
}
