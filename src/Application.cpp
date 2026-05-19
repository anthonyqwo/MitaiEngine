#include "Application.h"
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <GLFW/glfw3native.h>
#include <imm.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "ResourceManager.h"
#include "AssetPath.h"
#include "IBLBaker.h"
#include "RippleSystem.h"

Application* Application::s_instance = nullptr;

Application::Application() {
    s_instance = this;
    window = nullptr;
    scene = new Scene();
    renderer = nullptr;
    physicsSystem = new PhysicsSystem();
    
    deltaTime = 0.0f;
    lastFrame = 0.0f;
    cursorDisabled = false;
    lastX = SCR_WIDTH / 2.0f; 
    lastY = SCR_HEIGHT / 2.0f;
    firstMouse = true;

    useNormalMap = true;
    light2Moving = false;
    tessLevel = 4.0f;
    explosionFactor = 0.0f;
    pSpread = 2.0f; 
    pSize = 0.4f; 
    pCount = 256.0f;
    shadowBias = 0.005f;
    pcfRadius = 1.5f;
    selectedEntityIndex = -1;
}

Application::~Application() {
    delete scene;
    delete renderer;
    delete physicsSystem;
    ResourceManager::clear();
    delete helmetModel;
    delete houseModel;
    delete treeModel;
}

bool Application::init() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "PBR Engine V4.5 - Deferred Rendering", NULL, NULL);
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
    ResourceManager::loadShader("shaders/vertex.glsl", "shaders/gbuffer_f.glsl", nullptr, nullptr, nullptr, "gbuffer");
    ResourceManager::loadShader("shaders/adv_v.glsl", "shaders/adv_gbuffer_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl", "advGbuffer");
    ResourceManager::loadShader("shaders/deferred_lighting_v.glsl", "shaders/deferred_lighting_f.glsl", nullptr, nullptr, nullptr, "deferred_lighting");
    
    ResourceManager::loadShader("shaders/vertex.glsl", "shaders/forward_water_f.glsl", nullptr, nullptr, nullptr, "forward_water");
    ResourceManager::loadShader("shaders/vertex.glsl", "shaders/unlit_f.glsl", nullptr, nullptr, nullptr, "unlit");
    ResourceManager::loadComputeShader("shaders/wave_query.glsl", "waveQuery");
    ResourceManager::loadComputeShader("shaders/ripple_step.glsl", "rippleStep");
    ResourceManager::loadComputeShader("shaders/ripple_inject.glsl", "rippleInject");
    
    ResourceManager::loadShader("shaders/skybox_v.glsl", "shaders/skybox_f.glsl", nullptr, nullptr, nullptr, "skybox");
    ResourceManager::loadShader("shaders/shadow_v.glsl", "shaders/shadow_f.glsl", nullptr, nullptr, nullptr, "shadow");
    ResourceManager::loadShader("shaders/point_shadow_v.glsl", "shaders/point_shadow_f.glsl", "shaders/point_shadow_g.glsl", nullptr, nullptr, "pointShadow");
    ResourceManager::loadShader("shaders/particle_v.glsl", "shaders/particle_f.glsl", "shaders/particle_g.glsl", "shaders/particle_tc.glsl", "shaders/particle_te.glsl", "particle");
    ResourceManager::loadShader("shaders/adv_v.glsl", "shaders/shadow_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl", "advShadow");
    ResourceManager::loadShader("shaders/adv_v.glsl", "shaders/point_shadow_f.glsl", "shaders/adv_point_shadow_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl", "advPointShadow");
    ResourceManager::loadShader("shaders/debug_line_v.glsl", "shaders/debug_line_f.glsl", nullptr, nullptr, nullptr, "debugLine");

    ResourceManager::loadTexture("assets/container.jpg", "texDiff");
    ResourceManager::loadTexture("assets/container_specular.png", "texSpec");
    ResourceManager::loadTexture("assets/container_normal.png", "texNorm");
    ResourceManager::loadTexture("assets/pbr/cargo_metal_albedo.png", "cargoMetalAlbedo");
    ResourceManager::loadTexture("assets/pbr/cargo_metal_normal.png", "cargoMetalNormal");
    ResourceManager::loadTexture("assets/pbr/cargo_metal_metallic.png", "cargoMetalMetallic");
    ResourceManager::loadTexture("assets/pbr/cargo_metal_roughness.png", "cargoMetalRoughness");
    ResourceManager::loadTexture("assets/pbr/cargo_metal_ao.png", "cargoMetalAO");
    ResourceManager::loadTexture("assets/pbr/painted_box_albedo.png", "paintedBoxAlbedo");
    ResourceManager::loadTexture("assets/pbr/painted_box_normal.png", "paintedBoxNormal");
    ResourceManager::loadTexture("assets/pbr/painted_box_metallic.png", "paintedBoxMetallic");
    ResourceManager::loadTexture("assets/pbr/painted_box_roughness.png", "paintedBoxRoughness");
    ResourceManager::loadTexture("assets/pbr/painted_box_ao.png", "paintedBoxAO");
    ResourceManager::loadTexture("assets/pbr/wood_plank_albedo.png", "woodPlankAlbedo");
    ResourceManager::loadTexture("assets/pbr/wood_plank_normal.png", "woodPlankNormal");
    ResourceManager::loadTexture("assets/pbr/wood_plank_metallic.png", "woodPlankMetallic");
    ResourceManager::loadTexture("assets/pbr/wood_plank_roughness.png", "woodPlankRoughness");
    ResourceManager::loadTexture("assets/pbr/wood_plank_ao.png", "woodPlankAO");
    ResourceManager::loadTexture("assets/water_normal.jpg", "waterNorm");
    ResourceManager::loadTexture("assets/wall.jpg", "floorDiff");
    ResourceManager::loadTexture("assets/wall_normal.jpg", "floorNorm");
    // Save original floor textures under alternate names for scene switching
    ResourceManager::Textures["wallDiff"] = ResourceManager::getTexture("floorDiff");
    ResourceManager::Textures["wallNorm"] = ResourceManager::getTexture("floorNorm");
    ResourceManager::createSolidTexture(255, 255, 255, "whiteTex");
    ResourceManager::createSolidTexture(128, 128, 255, "flatNormalTex");
    ResourceManager::createSolidTexture(0, 0, 0, "blackTex");

    scene->skybox = loadCubemap({"assets/skybox/right.jpg","assets/skybox/left.jpg","assets/skybox/top.jpg","assets/skybox/bottom.jpg","assets/skybox/front.jpg","assets/skybox/back.jpg"});
    ResourceManager::Textures["skyboxMap"] = scene->skybox;

    renderer = new Renderer(SCR_WIDTH, SCR_HEIGHT);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    // Bake IBL
    // IBLBaker draws cubes internally, so it needs a basic cube VAO. Let's rebuild one locally just for baker if we didn't store it globally.
    // Wait, IBLBaker needs a cube VAO. We used to pass it cubeVAO from Application.
    // I'll create one local cube VAO here just for the baker.
    unsigned int bakerVao, bakerVbo;
    float cubeV[] = { -1,-1,-1, -1,-1,1, -1,1,1, -1,1,-1, 1,-1,-1, 1,1,-1, 1,1,1, 1,-1,1, -1,-1,-1, 1,-1,-1, 1,-1,1, -1,-1,1, -1,1,-1, -1,1,1, 1,1,1, 1,1,-1, -1,-1,-1, -1,1,-1, 1,1,-1, 1,-1,-1, -1,-1,1, 1,-1,1, 1,1,1, -1,1,1 };
    // actually just use skybox vertices 
    float skyV[] = { -1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,1,-1,1,-1,1,1,-1,1,1,1,1,1,1,-1,1,1,-1,1,-1,-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,-1,-1,-1,1,1,-1,1 };
    glGenVertexArrays(1, &bakerVao); glGenBuffers(1, &bakerVbo);
    glBindVertexArray(bakerVao); glBindBuffer(GL_ARRAY_BUFFER, bakerVbo); glBufferData(GL_ARRAY_BUFFER, sizeof(skyV), skyV, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    
    IBLBaker::bake(scene->skybox, bakerVao, scene->irradianceMap, scene->prefilterMap, scene->brdfLUT);
    ResourceManager::Textures["irradianceMap"] = scene->irradianceMap;
    ResourceManager::Textures["prefilterMap"] = scene->prefilterMap;
    ResourceManager::Textures["brdfLUT"] = scene->brdfLUT;

    helmetModel = new Model("assets/models/DamagedHelmet.glb");
    houseModel = new Model("assets/models/world/fantasy_house.glb");
    treeModel = new Model("assets/models/world/trees.glb");

    ResourceManager::loadTexture("assets/grass_diffuse.jpg", "grassDiff");
    ResourceManager::loadTexture("assets/grass_normal.jpg", "grassNorm");
    
    ResourceManager::getShader("skybox")->use(); 
    ResourceManager::getShader("skybox")->setInt("skybox", 0);
    
    ResourceManager::getShader("deferred_lighting")->use();
    ResourceManager::getShader("deferred_lighting")->setInt("gPosition", 0);
    ResourceManager::getShader("deferred_lighting")->setInt("gNormal", 1);
    ResourceManager::getShader("deferred_lighting")->setInt("gAlbedo", 2);
    ResourceManager::getShader("deferred_lighting")->setInt("gPBR", 3);
    ResourceManager::getShader("deferred_lighting")->setInt("shadowMap", 4);
    ResourceManager::getShader("deferred_lighting")->setInt("irradianceMap", 5);
    ResourceManager::getShader("deferred_lighting")->setInt("prefilterMap", 6);
    ResourceManager::getShader("deferred_lighting")->setInt("brdfLUT", 7);
    ResourceManager::getShader("deferred_lighting")->setInt("pointShadowMap", 8);
    ResourceManager::getShader("deferred_lighting")->setInt("gEmissiveMap", 9);
}

void Application::loadDefaultScene() {
    // Restore original floor textures
    ResourceManager::Textures["floorDiff"] = ResourceManager::getTexture("wallDiff");
    ResourceManager::Textures["floorNorm"] = ResourceManager::getTexture("wallNorm");
    scene->camera.Position = glm::vec3(0.0f, 2.0f, 5.0f);
    scene->camera.Pitch = 0.0f;
    scene->camera.Yaw = -90.0f;
    scene->camera.ProcessMouseMovement(0, 0);

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
    waterEntity.roughness=0.055f; waterEntity.reflectivity=0.42f; waterEntity.metallic=0.0f;
    waterEntity.hasCollision = false;
    scene->addEntity(waterEntity);

    Entity sunEnt("Main Sun", CUBE, glm::vec3(5.0f, 10.0f, 5.0f), glm::vec3(1.0f, 0.95f, 0.8f));
    sunEnt.isLight=true; sunEnt.lightColor=glm::vec3(1.0f, 0.95f, 0.8f); sunEnt.lightIntensity=5.0f; sunEnt.scale=glm::vec3(0.2f);
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

void Application::loadCollisionDemoScene() {
    // Restore original floor textures
    ResourceManager::Textures["floorDiff"] = ResourceManager::getTexture("wallDiff");
    ResourceManager::Textures["floorNorm"] = ResourceManager::getTexture("wallNorm");
    scene->camera.Position = glm::vec3(0.0f, 15.0f, 35.0f);
    scene->camera.Pitch = -30.0f;
    scene->camera.ProcessMouseMovement(0, 0); // update vectors
    
    // Light
    Entity sunEnt("Main Sun", CUBE, glm::vec3(0.0f, 20.5f, 0.0f), glm::vec3(1.0f));
    sunEnt.isLight = true; sunEnt.lightColor = glm::vec3(1.0f); sunEnt.lightIntensity = 5.0f; sunEnt.scale = glm::vec3(0.5f);
    sunEnt.hasCollision = false;
    scene->addEntity(sunEnt);

    // Floor (21x21 blocks, centered at 0. X = -10 to 10, Z = -10 to 10)
    for (int x = -10; x <= 10; ++x) {
        for (int z = -10; z <= 10; ++z) {
            bool isWhite = ((x + z) % 2 == 0);
            glm::vec3 col = isWhite ? glm::vec3(0.9f) : glm::vec3(0.1f, 0.2f, 0.8f);
            Entity block("FloorBlock", CUBE, glm::vec3(x, 0.0f, z), col);
            block.scale = glm::vec3(1.0f, 0.1f, 1.0f); // 1m x 1m, 0.1m height
            block.roughness = 0.8f; block.metallic = 0.1f; block.reflectivity = 0.05f;
            block.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
            block.mass = 0.0f; // static
            scene->addEntity(block);
        }
    }
    
    // Walls
    Entity walls[4] = {
        Entity("LeftWall", CUBE, glm::vec3(-11.0f, 10.5f, 0.0f), glm::vec3(0.2f, 0.8f, 0.2f)),
        Entity("RightWall", CUBE, glm::vec3(11.0f, 10.5f, 0.0f), glm::vec3(0.8f, 0.8f, 0.2f)),
        Entity("BackWall", CUBE, glm::vec3(0.0f, 10.5f, -11.0f), glm::vec3(0.2f, 0.8f, 0.2f)),
        Entity("FrontWall", CUBE, glm::vec3(0.0f, 10.5f, 11.0f), glm::vec3(0.8f, 0.8f, 0.2f))
    };
    walls[0].scale = glm::vec3(1.0f, 21.0f, 23.0f);
    walls[1].scale = glm::vec3(1.0f, 21.0f, 23.0f);
    walls[2].scale = glm::vec3(21.0f, 21.0f, 1.0f);
    walls[3].scale = glm::vec3(21.0f, 21.0f, 1.0f);
    
    for (int i=0; i<4; i++) {
        walls[i].localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        walls[i].mass = 0.0f;
        walls[i].roughness = 0.5f;
        scene->addEntity(walls[i]);
    }

    // Objects on floor (size > 3x3x3).
    Entity obj1("BigBox1", CUBE, glm::vec3(-5.0f, 2.0f, -5.0f), glm::vec3(0.8f, 0.2f, 0.2f));
    obj1.scale = glm::vec3(4.0f, 4.0f, 4.0f); obj1.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); obj1.mass = 0.0f; scene->addEntity(obj1);

    Entity obj2("BigBox2", CUBE, glm::vec3(5.0f, 2.0f, -5.0f), glm::vec3(0.2f, 0.8f, 0.8f));
    obj2.scale = glm::vec3(4.0f, 4.0f, 4.0f); obj2.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); obj2.mass = 0.0f; scene->addEntity(obj2);

    Entity obj3("BigBox3", CUBE, glm::vec3(-5.0f, 2.0f, 5.0f), glm::vec3(0.8f, 0.2f, 0.8f));
    obj3.scale = glm::vec3(4.0f, 4.0f, 4.0f); obj3.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); obj3.mass = 0.0f; scene->addEntity(obj3);

    Entity obj4("BigBox4", CUBE, glm::vec3(5.0f, 2.0f, 5.0f), glm::vec3(0.8f, 0.5f, 0.2f));
    obj4.scale = glm::vec3(4.0f, 4.0f, 4.0f); obj4.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); obj4.mass = 0.0f; scene->addEntity(obj4);
}

void Application::loadWorldScene() {
    // Swap floor textures to grass for this scene
    ResourceManager::Textures["floorDiff"] = ResourceManager::getTexture("grassDiff");
    ResourceManager::Textures["floorNorm"] = ResourceManager::getTexture("grassNorm");

    // Camera: start elevated, looking toward the house
    scene->camera.Position = glm::vec3(8.0f, 3.0f, 12.0f);
    scene->camera.Pitch = -10.0f;
    scene->camera.Yaw = -110.0f;
    scene->camera.ProcessMouseMovement(0, 0);

    // === Lighting ===
    Entity sunEnt("Main Sun", CUBE, glm::vec3(15.0f, 20.0f, 10.0f), glm::vec3(1.0f, 0.95f, 0.85f));
    sunEnt.isLight = true; sunEnt.lightColor = glm::vec3(1.0f, 0.95f, 0.85f);
    sunEnt.lightIntensity = 5.0f; sunEnt.scale = glm::vec3(0.3f);
    sunEnt.hasCollision = false;
    scene->addEntity(sunEnt);

    // Interior point light inside house
    Entity lampEnt("Point Light", CUBE, glm::vec3(0.0f, 2.5f, 0.0f), glm::vec3(1.0f, 0.8f, 0.4f));
    lampEnt.isLight = true; lampEnt.lightColor = glm::vec3(1.0f, 0.8f, 0.4f);
    lampEnt.lightIntensity = 3.0f; lampEnt.scale = glm::vec3(0.15f);
    lampEnt.hasCollision = false;
    scene->addEntity(lampEnt);

    // === Ground Terrain (large grass-textured floor) ===
    // Floor geometry Y=0 locally, positioned at Y=-0.5 like Water Demo
    Entity groundEnt("Terrain", FLOOR, glm::vec3(0, -0.5f, 0), glm::vec3(0.6f, 0.8f, 0.4f));
    groundEnt.scale = glm::vec3(5.0f, 1.0f, 5.0f);
    groundEnt.roughness = 0.9f; groundEnt.metallic = 0.0f;
    groundEnt.ambient = 1.0f; groundEnt.reflectivity = 0.02f;
    groundEnt.hasCollision = false;
    scene->addEntity(groundEnt);

    // === Water Lake (same approach as Water Demo: water Y just above floor Y) ===
    Entity waterEnt("Water Surface", WATER, glm::vec3(-10.0f, -0.49f, -8.0f), glm::vec3(0.1f, 0.3f, 0.6f));
    waterEnt.scale = glm::vec3(1.5f, 1.0f, 1.5f);
    waterEnt.roughness = 0.055f; waterEnt.reflectivity = 0.42f; waterEnt.metallic = 0.0f;
    waterEnt.hasCollision = false;
    scene->addEntity(waterEnt);

    // === House (Fantasy House GLB) ===
    Entity houseEnt("House", MODEL, glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(1.0f));
    houseEnt.model = houseModel;
    houseEnt.scale = glm::vec3(3.0f);
    houseEnt.metallic = 0.1f; houseEnt.roughness = 0.8f;
    houseEnt.ambient = 1.0f; houseEnt.reflectivity = 0.05f;
    houseEnt.localBounds = houseModel->localBounds;
    houseEnt.hasCollision = false; // allow camera to enter
    scene->addEntity(houseEnt);

    // === Trees (scattered across terrain) ===
    struct TreePlacement { float x, z, rotY, sc; };
    TreePlacement treePlacements[] = {
        { 8.0f,  10.0f,  45.0f, 1.5f},   {-5.0f,  12.0f, 120.0f, 1.8f},
        {10.0f,  -6.0f, 200.0f, 1.6f},   {-10.0f,  4.0f,  80.0f, 1.4f},
        {15.0f,   4.0f, 160.0f, 2.0f},   {-6.0f, -15.0f, 300.0f, 1.5f},
        { 5.0f, -12.0f,  30.0f, 1.7f},   {16.0f,  12.0f, 250.0f, 1.3f},
        {-15.0f, 10.0f, 140.0f, 1.9f},   {-16.0f,-12.0f,  60.0f, 1.6f},
        {12.0f, -14.0f, 180.0f, 1.2f},   { 3.0f,  16.0f, 350.0f, 2.0f},
    };
    for (int i = 0; i < 12; i++) {
        const auto& tp = treePlacements[i];
        Entity treeEnt("Tree_" + std::to_string(i), MODEL, glm::vec3(tp.x, -0.5f, tp.z), glm::vec3(0.5f, 0.7f, 0.3f));
        treeEnt.model = treeModel;
        treeEnt.scale = glm::vec3(tp.sc);
        treeEnt.rotation.y = tp.rotY;
        treeEnt.metallic = 0.0f; treeEnt.roughness = 0.9f;
        treeEnt.ambient = 1.0f; treeEnt.reflectivity = 0.02f;
        treeEnt.localBounds = treeModel->localBounds;
        treeEnt.hasCollision = false;
        scene->addEntity(treeEnt);
    }

    // === Hills (terrain mounds using scaled spheres, poke ABOVE ground) ===
    struct HillPlacement { float x, z, sx, sy, sz; glm::vec3 col; };
    HillPlacement hills[] = {
        { 20.0f,  0.0f,  7.0f, 3.0f, 5.0f, glm::vec3(0.35f, 0.5f, 0.25f)},
        {-20.0f,  8.0f,  5.0f, 2.0f, 6.0f, glm::vec3(0.3f, 0.45f, 0.2f)},
        {  8.0f,-20.0f,  8.0f, 3.5f, 7.0f, glm::vec3(0.4f, 0.55f, 0.3f)},
        {-18.0f,-16.0f,  6.0f, 2.5f, 4.0f, glm::vec3(0.32f, 0.48f, 0.22f)},
        { 16.0f, 16.0f,  4.0f, 1.8f, 5.0f, glm::vec3(0.38f, 0.52f, 0.28f)},
    };
    for (int i = 0; i < 5; i++) {
        const auto& h = hills[i];
        // Y = -0.5 + sy*0.3 so the sphere pokes above ground
        Entity hillEnt("Hill_" + std::to_string(i), SPHERE, glm::vec3(h.x, -0.5f + h.sy * 0.3f, h.z), h.col);
        hillEnt.scale = glm::vec3(h.sx, h.sy, h.sz);
        hillEnt.roughness = 0.95f; hillEnt.metallic = 0.0f;
        hillEnt.ambient = 1.0f; hillEnt.reflectivity = 0.01f;
        hillEnt.localBounds = AABB(glm::vec3(-1.0f), glm::vec3(1.0f));
        hillEnt.hasCollision = false;
        scene->addEntity(hillEnt);
    }

    // === Rocks (scattered on the ground surface) ===
    struct RockPlacement { float x, z, rotY, sc; };
    RockPlacement rocks[] = {
        {-12.0f, -6.0f, 35.0f, 0.8f},
        { -6.0f, -10.0f, 120.0f, 0.5f},
        { 14.0f,  6.0f, 200.0f, 0.6f},
        {  3.0f, -16.0f, 80.0f, 0.7f},
        {-14.0f, 14.0f, 260.0f, 0.9f},
        { 18.0f, -8.0f, 150.0f, 0.4f},
    };
    for (int i = 0; i < 6; i++) {
        const auto& r = rocks[i];
        // Rocks sit ON the ground: Y = -0.5 + half height
        float halfH = r.sc * 0.6f * 0.5f;
        Entity rockEnt("Rock_" + std::to_string(i), SPHERE, glm::vec3(r.x, -0.5f + halfH, r.z), glm::vec3(0.45f, 0.42f, 0.38f));
        rockEnt.scale = glm::vec3(r.sc, r.sc * 0.6f, r.sc * 0.8f);
        rockEnt.rotation.y = r.rotY;
        rockEnt.roughness = 0.95f; rockEnt.metallic = 0.0f;
        rockEnt.ambient = 1.0f; rockEnt.reflectivity = 0.03f;
        rockEnt.localBounds = AABB(glm::vec3(-1.0f), glm::vec3(1.0f));
        rockEnt.hasCollision = false;
        scene->addEntity(rockEnt);
    }

    // === Decorative wooden fences near house ===
    for (int i = -3; i <= 3; i++) {
        Entity fencePost("Fence_" + std::to_string(i + 3), CUBE, glm::vec3(i * 1.5f, -0.1f, 5.0f), glm::vec3(0.55f, 0.35f, 0.15f));
        fencePost.scale = glm::vec3(0.15f, 0.6f, 0.15f);
        fencePost.roughness = 0.9f; fencePost.metallic = 0.0f;
        fencePost.ambient = 1.0f; fencePost.reflectivity = 0.02f;
        fencePost.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        fencePost.hasCollision = false;
        scene->addEntity(fencePost);
    }
    Entity fenceRail("FenceRail", CUBE, glm::vec3(0.0f, 0.05f, 5.0f), glm::vec3(0.55f, 0.35f, 0.15f));
    fenceRail.scale = glm::vec3(10.0f, 0.08f, 0.1f);
    fenceRail.roughness = 0.9f; fenceRail.metallic = 0.0f;
    fenceRail.ambient = 1.0f; fenceRail.reflectivity = 0.02f;
    fenceRail.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    fenceRail.hasCollision = false;
    scene->addEntity(fenceRail);
}

void Application::loadBuoyancyScene(int scenario) {
    physicsSystem->reset();
    scene->entities.clear();
    selectedEntityIndex = -1;
    isCollisionDemo = false;
    isBuoyancyScene = true;
    currentScenario = scenario;

    // 1. Camera setup: look down at the buoyancy tank
    scene->camera.Position = glm::vec3(0.0f, 7.5f, 11.5f);
    scene->camera.Pitch = -30.0f;
    scene->camera.Yaw = -90.0f;
    scene->camera.ProcessMouseMovement(0, 0);

    // 2. Spawn Glass Tank bottom and walls (10x10x5m, centered at (0, 2.5, 0))
    // Bottom
    Entity tankBottom("Tank Bottom", CUBE, glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(0.85f, 0.95f, 1.0f));
    tankBottom.scale = glm::vec3(10.2f, 0.1f, 10.2f);
    tankBottom.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    tankBottom.mass = 0.0f;
    tankBottom.roughness = 0.02f; tankBottom.metallic = 0.0f; tankBottom.reflectivity = 0.8f;
    scene->addEntity(tankBottom);

    // Left Wall
    Entity tankLeft("Tank Left", CUBE, glm::vec3(-5.05f, 2.5f, 0.0f), glm::vec3(0.85f, 0.95f, 1.0f));
    tankLeft.scale = glm::vec3(0.1f, 5.0f, 10.2f);
    tankLeft.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    tankLeft.mass = 0.0f;
    tankLeft.roughness = 0.02f; tankLeft.metallic = 0.0f; tankLeft.reflectivity = 0.8f;
    scene->addEntity(tankLeft);

    // Right Wall
    Entity tankRight("Tank Right", CUBE, glm::vec3(5.05f, 2.5f, 0.0f), glm::vec3(0.85f, 0.95f, 1.0f));
    tankRight.scale = glm::vec3(0.1f, 5.0f, 10.2f);
    tankRight.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    tankRight.mass = 0.0f;
    tankRight.roughness = 0.02f; tankRight.metallic = 0.0f; tankRight.reflectivity = 0.8f;
    scene->addEntity(tankRight);

    // Back Wall
    Entity tankBack("Tank Back", CUBE, glm::vec3(0.0f, 2.5f, -5.05f), glm::vec3(0.85f, 0.95f, 1.0f));
    tankBack.scale = glm::vec3(10.0f, 5.0f, 0.1f);
    tankBack.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    tankBack.mass = 0.0f;
    tankBack.roughness = 0.02f; tankBack.metallic = 0.0f; tankBack.reflectivity = 0.8f;
    scene->addEntity(tankBack);

    // Front Wall
    Entity tankFront("Tank Front", CUBE, glm::vec3(0.0f, 2.5f, 5.05f), glm::vec3(0.85f, 0.95f, 1.0f));
    tankFront.scale = glm::vec3(10.0f, 5.0f, 0.1f);
    tankFront.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    tankFront.mass = 0.0f;
    tankFront.roughness = 0.02f; tankFront.metallic = 0.0f; tankFront.reflectivity = 0.8f;
    scene->addEntity(tankFront);

    // 3. Spawn Water plane (y = 4.0m)
    Entity waterEntity("Water Surface", WATER, glm::vec3(0.0f, 4.0f, 0.0f), glm::vec3(0.0f, 0.4f, 0.8f));
    waterEntity.roughness = 0.045f; waterEntity.reflectivity = 0.46f; waterEntity.metallic = 0.0f;
    waterEntity.ambient = 1.0f;
    waterEntity.scale = glm::vec3(10.0f / 14.0f, 1.0f, 10.0f / 14.0f);
    waterEntity.hasCollision = false;
    scene->addEntity(waterEntity);

    // 4. Spawn Lights (Main Sun and Point Light)
    Entity sunEnt("Main Sun", CUBE, glm::vec3(8.0f, 12.0f, 8.0f), glm::vec3(1.0f, 0.95f, 0.8f));
    sunEnt.isLight = true; sunEnt.lightColor = glm::vec3(1.0f, 0.95f, 0.8f);
    sunEnt.lightIntensity = 6.0f; sunEnt.scale = glm::vec3(0.3f);
    sunEnt.hasCollision = false;
    scene->addEntity(sunEnt);

    Entity lampEnt("Point Light", CUBE, glm::vec3(0.0f, 6.8f, 0.0f), glm::vec3(1.0f, 0.6f, 0.2f));
    lampEnt.isLight = true; lampEnt.lightColor = glm::vec3(1.0f, 0.6f, 0.2f);
    lampEnt.lightIntensity = 3.0f; lampEnt.scale = glm::vec3(0.2f);
    lampEnt.hasCollision = false;
    scene->addEntity(lampEnt);

    // 5. Spawn Buoyant Entities based on Scenario A/B/C/D
    if (scenario == 0 || scenario == 3) { // Hollow Sphere (Scenario A or D)
        glm::vec3 pos = (scenario == 3) ? glm::vec3(-2.0f, 6.0f, 0.0f) : glm::vec3(0.0f, 6.0f, 0.0f);
        Entity sphereEnt("Hollow Sphere", SPHERE, pos, glm::vec3(0.8f, 0.85f, 0.9f));
        sphereEnt.scale = glm::vec3(0.5f); // radius = 0.5m
        sphereEnt.localBounds = AABB(glm::vec3(-1.0f), glm::vec3(1.0f));
        sphereEnt.roughness = 0.1f; sphereEnt.metallic = 0.9f; sphereEnt.reflectivity = 0.8f; sphereEnt.ambient = 1.0f;
        
        sphereEnt.isBuoyant = true;
        sphereEnt.buoyancyType = 0; // Sphere
        sphereEnt.mass = 157.08f;
        sphereEnt.radius = 0.5f;
        sphereEnt.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        
        scene->addEntity(sphereEnt);
    }
    if (scenario == 1 || scenario == 3) { // Wooden Slab (Scenario B or D)
        glm::vec3 pos = (scenario == 3) ? glm::vec3(0.0f, 5.0f, 0.0f) : glm::vec3(0.0f, 5.0f, 0.0f);
        Entity slabEnt("Wooden Slab", CUBE, pos, glm::vec3(0.65f, 0.45f, 0.25f));
        slabEnt.scale = glm::vec3(1.0f, 0.3f, 1.0f);
        slabEnt.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        slabEnt.color = glm::vec3(1.0f);
        slabEnt.originalColor = slabEnt.color;
        slabEnt.roughness = 1.0f; slabEnt.metallic = 1.0f; slabEnt.reflectivity = 0.04f; slabEnt.ambient = 1.0f;
        slabEnt.albedoTexture = "woodPlankAlbedo";
        slabEnt.normalTexture = "woodPlankNormal";
        slabEnt.metallicTexture = "woodPlankMetallic";
        slabEnt.roughnessTexture = "woodPlankRoughness";
        slabEnt.aoTexture = "woodPlankAO";
        
        slabEnt.isBuoyant = true;
        slabEnt.buoyancyType = 1; // Slab
        slabEnt.mass = 120.0f;
        // Initial Tilt
        slabEnt.rotation = glm::vec3(30.0f, 0.0f, 15.0f);
        slabEnt.orientation = glm::quat(glm::radians(slabEnt.rotation));
        
        scene->addEntity(slabEnt);
    }
    if (scenario == 2 || scenario == 3) { // Open-top Box (Scenario C or D)
        glm::vec3 pos = (scenario == 3) ? glm::vec3(2.0f, 4.15f, 0.0f) : glm::vec3(0.0f, 4.15f, 0.0f);
        Entity boxEnt("Open Box", CUBE, pos, glm::vec3(0.85f, 0.65f, 0.15f));
        boxEnt.scale = glm::vec3(1.2f, 0.6f, 1.2f);
        boxEnt.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        boxEnt.color = glm::vec3(1.0f);
        boxEnt.originalColor = boxEnt.color;
        boxEnt.roughness = 1.0f; boxEnt.metallic = 1.0f; boxEnt.reflectivity = 0.45f; boxEnt.ambient = 1.0f;
        boxEnt.albedoTexture = "paintedBoxAlbedo";
        boxEnt.normalTexture = "paintedBoxNormal";
        boxEnt.metallicTexture = "paintedBoxMetallic";
        boxEnt.roughnessTexture = "paintedBoxRoughness";
        boxEnt.aoTexture = "paintedBoxAO";
        
        boxEnt.isBuoyant = true;
        boxEnt.buoyancyType = 2; // Open-top box
        boxEnt.mass = 150.0f;
        boxEnt.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        
        scene->addEntity(boxEnt);

        // Spawn Cargo inside the Open Box!
        // Random horizontal offset within the inner compartment
        float rx = ((rand() % 200) / 100.0f - 1.0f) * 0.08f; // [-0.08, 0.08]
        float rz = ((rand() % 200) / 100.0f - 1.0f) * 0.08f; // [-0.08, 0.08]
        glm::vec3 localOffset(rx, -0.12f, rz); // Sitting perfectly on the bottom floor (Y = -0.12)
        
        Entity cargoEnt("Box Cargo", CUBE, pos + localOffset, glm::vec3(0.45f, 0.45f, 0.5f));
        cargoEnt.scale = glm::vec3(0.3f); // Small heavy metal block
        cargoEnt.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        cargoEnt.color = glm::vec3(1.0f);
        cargoEnt.originalColor = cargoEnt.color;
        cargoEnt.roughness = 1.0f; cargoEnt.metallic = 1.0f; cargoEnt.reflectivity = 0.65f; cargoEnt.ambient = 1.0f;
        cargoEnt.albedoTexture = "cargoMetalAlbedo";
        cargoEnt.normalTexture = "cargoMetalNormal";
        cargoEnt.metallicTexture = "cargoMetalMetallic";
        cargoEnt.roughnessTexture = "cargoMetalRoughness";
        cargoEnt.aoTexture = "cargoMetalAO";
        
        cargoEnt.isCargo = true;
        cargoEnt.buoyancyType = 1; // Box-shaped if it becomes a standalone buoyant body.
        cargoEnt.cargoLocalOffset = localOffset;
        cargoEnt.mass = 75.0f; // Initial cargo mass
        cargoEnt.hasCollision = false; // No separate collision
        
        scene->addEntity(cargoEnt);
    }
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

    // Buoyancy Scenario Keyboard Triggers (1, 2, 3, 4)
    if (!cursorDisabled) {
        static bool key1P = false;
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !key1P) {
            loadBuoyancyScene(0);
            key1P = true;
        } else if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) {
            key1P = false;
        }

        static bool key2P = false;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !key2P) {
            loadBuoyancyScene(1);
            key2P = true;
        } else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) {
            key2P = false;
        }

        static bool key3P = false;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !key3P) {
            loadBuoyancyScene(2);
            key3P = true;
        } else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE) {
            key3P = false;
        }

        static bool key4P = false;
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !key4P) {
            loadBuoyancyScene(3);
            key4P = true;
        } else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE) {
            key4P = false;
        }
    }

    static bool keyF1P = false;
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && !keyF1P) {
        debugBuoyancy = !debugBuoyancy;
        keyF1P = true;
    } else if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) {
        keyF1P = false;
    }

    static bool keyF2P = false;
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS && !keyF2P) {
        showProfilingOverlay = !showProfilingOverlay;
        keyF2P = true;
    } else if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_RELEASE) {
        keyF2P = false;
    }

    static bool keyF3P = false;
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS && !keyF3P) {
        gbufferVisualisationMode = (gbufferVisualisationMode + 1) % 6;
        keyF3P = true;
    } else if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_RELEASE) {
        keyF3P = false;
    }

    static bool keyF4P = false;
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS && !keyF4P) {
        waterWavesEnabled = !waterWavesEnabled;
        keyF4P = true;
    } else if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_RELEASE) {
        keyF4P = false;
    }

    static bool keyF5P = false;
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS && !keyF5P) {
        waterDebugMode = (waterDebugMode + 1) % 22;
        keyF5P = true;
    } else if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_RELEASE) {
        keyF5P = false;
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

    // 3D Mouse Grabbing & Viewport Drag Plane Controller
    if (!cursorDisabled) {
        static bool lastMousePressed = false;
        bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        
        static glm::vec3 dragPlanePoint = glm::vec3(0.0f);
        static glm::vec3 dragPlaneNormal = glm::vec3(0.0f);
        
        if (mousePressed && !ImGui::GetIO().WantCaptureMouse) {
            bool grabbing = false;
            for (auto& entity : scene->entities) {
                if (entity.isGrabbed) {
                    grabbing = true;
                    break;
                }
            }
            
            if (!grabbing && !lastMousePressed) {
                // Initial press: cast screen ray
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                int width, height;
                glfwGetWindowSize(window, &width, &height);

                float x_ndc = (2.0f * (float)xpos) / (float)width - 1.0f;
                float y_ndc = 1.0f - (2.0f * (float)ypos) / (float)height;

                glm::mat4 projection = glm::perspective(glm::radians(scene->camera.Zoom), (float)width / (float)height, 0.1f, 100.0f);
                glm::mat4 view = scene->camera.GetViewMatrix();

                glm::vec4 ray_clip = glm::vec4(x_ndc, y_ndc, -1.0f, 1.0f);
                glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
                ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

                glm::vec3 ray_world = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
                glm::vec3 ray_origin = scene->camera.Position;
                
                float closestT = 1e9f;
                Entity* closestEntity = nullptr;
                glm::vec3 closestHitPoint = glm::vec3(0.0f);
                
                for (auto& entity : scene->entities) {
                    if (entity.isBuoyant || entity.isCargo) {
                        float tHit = 0.0f;
                        glm::vec3 hitPoint = glm::vec3(0.0f);
                        
                        glm::mat4 modelMat = entity.getModelMatrix();
                        glm::mat4 invModel = glm::inverse(modelMat);

                        glm::vec3 oLocal = glm::vec3(invModel * glm::vec4(ray_origin, 1.0f));
                        // Normalize local direction to prevent non-uniform scaling OBB distortion
                        glm::vec3 dLocal = glm::normalize(glm::vec3(invModel * glm::vec4(ray_world, 0.0f)));

                        glm::vec3 minLocal = entity.localBounds.minExtents;
                        glm::vec3 maxLocal = entity.localBounds.maxExtents;

                        float tMin = -1e9f;
                        float tMax = 1e9f;
                        bool intersect = true;

                        for (int i = 0; i < 3; ++i) {
                            if (glm::abs(dLocal[i]) < 1e-6f) {
                                if (oLocal[i] < minLocal[i] || oLocal[i] > maxLocal[i]) {
                                    intersect = false;
                                    break;
                                }
                            } else {
                                float t1 = (minLocal[i] - oLocal[i]) / dLocal[i];
                                float t2 = (maxLocal[i] - oLocal[i]) / dLocal[i];
                                if (t1 > t2) std::swap(t1, t2);
                                tMin = (glm::max)(tMin, t1);
                                tMax = (glm::min)(tMax, t2);
                            }
                        }

                        if (intersect && tMax >= tMin && tMax > 0.0f) {
                            // Support ray origin inside the OBB box (tMin is negative)
                            float actualT = (tMin < 0.0f) ? tMax : tMin;
                            if (actualT > 0.0f && actualT < closestT) {
                                closestT = actualT;
                                closestEntity = &entity;
                                closestHitPoint = glm::vec3(modelMat * glm::vec4(oLocal + actualT * dLocal, 1.0f));
                            }
                        }
                    }
                }
                
                if (closestEntity) {
                    closestEntity->isGrabbed = true;
                    // Compute local grab offset offset relative to the body orientation
                    closestEntity->localGrabOffset = glm::transpose(glm::mat3_cast(closestEntity->orientation)) * (closestHitPoint - closestEntity->position);
                    closestEntity->targetGrabWorld = closestHitPoint;
                    
                    // Anchor camera-aligned virtual drag plane at initial intersection point
                    dragPlanePoint = closestHitPoint;
                    dragPlaneNormal = scene->camera.Front; 
                }
            }
            
            // Continuous Dragging Phase: Project cursor onto the virtual drag plane
            for (auto& entity : scene->entities) {
                if (entity.isGrabbed) {
                    double xpos, ypos;
                    glfwGetCursorPos(window, &xpos, &ypos);
                    int width, height;
                    glfwGetWindowSize(window, &width, &height);

                    float x_ndc = (2.0f * (float)xpos) / (float)width - 1.0f;
                    float y_ndc = 1.0f - (2.0f * (float)ypos) / (float)height;

                    glm::mat4 projection = glm::perspective(glm::radians(scene->camera.Zoom), (float)width / (float)height, 0.1f, 100.0f);
                    glm::mat4 view = scene->camera.GetViewMatrix();

                    glm::vec4 ray_clip = glm::vec4(x_ndc, y_ndc, -1.0f, 1.0f);
                    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
                    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

                    glm::vec3 ray_world = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
                    glm::vec3 ray_origin = scene->camera.Position;
                    
                    // Ray-Plane Intersection: t = (PlanePoint - RayOrigin) . Normal / RayDir . Normal
                    float denom = glm::dot(ray_world, dragPlaneNormal);
                    if (glm::abs(denom) > 1e-6f) {
                        float tDrag = glm::dot(dragPlanePoint - ray_origin, dragPlaneNormal) / denom;
                        if (tDrag > 0.0f) {
                            entity.targetGrabWorld = ray_origin + tDrag * ray_world;
                        }
                    }
                }
            }
            
            lastMousePressed = true;
        } else {
            // Release spring constraint
            for (auto& entity : scene->entities) {
                entity.isGrabbed = false;
            }
            lastMousePressed = false;
        }
    }
}

void Application::renderImGui() {
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

    // Draw continuous flooding overlay above the open box!
    if (debugBuoyancy) {
        for (const auto& e : scene->entities) {
            if (e.isBuoyant && e.buoyancyType == 2 && e.visible) {
                float H = e.scale.y;
                glm::vec3 topCenterWorld = e.position + glm::vec3(0.0f, H * 0.5f + 0.35f, 0.0f);
                
                glm::mat4 view = scene->camera.GetViewMatrix();
                glm::mat4 proj = glm::perspective(glm::radians(scene->camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
                glm::vec4 clipPos = proj * view * glm::vec4(topCenterWorld, 1.0f);
                
                if (clipPos.w > 0.0f) {
                    glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
                    float screenX = (ndcPos.x * 0.5f + 0.5f) * SCR_WIDTH;
                    float screenY = ((1.0f - ndcPos.y) * 0.5f + 0.5f) * SCR_HEIGHT;
                    
                    ImGui::SetNextWindowPos(ImVec2(screenX - 70.0f, screenY - 25.0f));
                    ImGui::SetNextWindowSize(ImVec2(140.0f, 48.0f));
                    ImGui::Begin(("##FloodWindow_" + e.name).c_str(), nullptr, 
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | 
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
                    
                    glm::vec3 colorVal;
                    if (e.floodLevel < 0.5f) {
                        float t = e.floodLevel * 2.0f;
                        colorVal = glm::mix(glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f), t);
                    } else {
                        float t = (e.floodLevel - 0.5f) * 2.0f;
                        colorVal = glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.2f, 0.2f), t);
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(colorVal.x, colorVal.y, colorVal.z, 1.0f));
                    ImGui::Text("Flood: %.1f%%", e.floodLevel * 100.0f);
                    ImGui::PopStyleColor();
                    
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(colorVal.x, colorVal.y, colorVal.z, 1.0f));
                    ImGui::ProgressBar(e.floodLevel, ImVec2(110.0f, 5.0f), "");
                    ImGui::PopStyleColor();
                    
                    ImGui::End();
                }
            }
        }
    }

    { ImGui::Begin("Scene Hierarchy");
      if (ImGui::Button("Add Cube")) { Entity e("New Cube", CUBE, scene->camera.Position + scene->camera.Front*2.0f); e.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f)); scene->addEntity(e); }
      ImGui::SameLine(); if (ImGui::Button("Add Sphere")) { Entity e("New Sphere", SPHERE, scene->camera.Position + scene->camera.Front*2.0f); e.localBounds = AABB(glm::vec3(-1), glm::vec3(1)); scene->addEntity(e); }
      ImGui::SameLine(); if (ImGui::Button("Add Adv Sphere")) { Entity e("Adv Sphere", ADV_SPHERE, scene->camera.Position + scene->camera.Front*2.0f, glm::vec3(0.8,0.2,0.5)); e.localBounds = AABB(glm::vec3(-1), glm::vec3(1)); scene->addEntity(e); }
      ImGui::SameLine(); if (ImGui::Button("Add Water")) { Entity w("Water", WATER, scene->camera.Position + scene->camera.Front*5.0f, glm::vec3(0.1,0.4,0.8)); w.roughness=0.05f; w.reflectivity=0.4f; w.hasCollision=false; scene->addEntity(w); }
      ImGui::SameLine(); if (ImGui::Button("Add Sun")) { Entity s("Main Sun", CUBE, scene->camera.Position + scene->camera.Front*5.0f, glm::vec3(1,0.95,0.8)); s.isLight=true; s.lightColor=glm::vec3(1,0.95,0.8); s.lightIntensity=2.0f; s.scale=glm::vec3(0.2f); s.hasCollision=false; scene->addEntity(s); }
      ImGui::SameLine(); if (ImGui::Button("Add Light")) { Entity p("Point Light", CUBE, scene->camera.Position + scene->camera.Front*3.0f, glm::vec3(1,0.5,0)); p.isLight=true; p.lightColor=glm::vec3(1,0.5,0); p.lightIntensity=2.0f; p.scale=glm::vec3(0.2f); p.hasCollision=false; scene->addEntity(p); }
      ImGui::Separator();
      for (int i=0; i<scene->entities.size(); i++) if (ImGui::Selectable((scene->entities[i].name + "##" + std::to_string(i)).c_str(), selectedEntityIndex == i)) selectedEntityIndex = i;
      ImGui::End(); }
    { ImGui::Begin("Inspector");
      if (selectedEntityIndex >= 0 && selectedEntityIndex < scene->entities.size()) {
          Entity& e = scene->entities[selectedEntityIndex];
          ImGui::Checkbox("Visible", &e.visible);
          ImGui::SliderFloat3("Position", glm::value_ptr(e.position), -15.0f, 15.0f);
          if (e.isBuoyant) {
              ImGui::Separator();
              ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Buoyancy Object State");
              ImGui::Text("Submerged: %.1f%%", e.submergedFraction * 100.0f);
              ImGui::Text("Flood Level: %.1f%%", e.floodLevel * 100.0f);
              ImGui::Text("Velocity: (%.2f, %.2f, %.2f) m/s", e.velocity.x, e.velocity.y, e.velocity.z);
              ImGui::Text("AngVel: (%.2f, %.2f, %.2f) rad/s", e.angularVelocity.x, e.angularVelocity.y, e.angularVelocity.z);
              
              ImGui::Separator();
              ImGui::Text("Object Adjustment");
              ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Tip: Ctrl+Click to type precise values!");
              ImGui::SliderFloat("Mass (kg)", &e.mass, 1.0f, 1000.0f);
              ImGui::SliderFloat3("Scale", glm::value_ptr(e.scale), 0.1f, 5.0f);
              if (e.buoyancyType == 2) {
                  ImGui::SliderFloat("Force Flood Level", &e.floodLevel, 0.0f, 1.0f, "%.3f");
              }
              
              ImGui::ColorEdit3("Col", glm::value_ptr(e.color));
              ImGui::Separator(); ImGui::Text("PBR Material");
              ImGui::SliderFloat("Roughness", &e.roughness, 0.05f, 1.0f);
              ImGui::SliderFloat("Metallic", &e.metallic, 0.0f, 1.0f);
              ImGui::SliderFloat("Ambient (AO)", &e.ambient, 0.0f, 1.0f);
              ImGui::SliderFloat("Reflectivity", &e.reflectivity, 0.0f, 1.0f);
          } else if (e.isCargo) {
              ImGui::Separator();
              ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Box Cargo Properties");
              ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Tip: Ctrl+Click to type precise values!");
              ImGui::SliderFloat("Mass (kg)", &e.mass, 1.0f, 800.0f);
              
              ImGui::SliderFloat("Local Offset X", &e.cargoLocalOffset.x, -0.4f, 0.4f);
              ImGui::SliderFloat("Local Offset Z", &e.cargoLocalOffset.z, -0.4f, 0.4f);
              
              ImGui::ColorEdit3("Col", glm::value_ptr(e.color));
              ImGui::Separator(); ImGui::Text("PBR Material");
              ImGui::SliderFloat("Roughness", &e.roughness, 0.05f, 1.0f);
              ImGui::SliderFloat("Metallic", &e.metallic, 0.0f, 1.0f);
              ImGui::SliderFloat("Ambient (AO)", &e.ambient, 0.0f, 1.0f);
              ImGui::SliderFloat("Reflectivity", &e.reflectivity, 0.0f, 1.0f);
          } else if (!e.isLight) {
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
      ImGui::Text("Active Scene");
      static int currentSceneIdx = 3;
      if (isCollisionDemo) {
          currentSceneIdx = 1;
      } else if (isBuoyancyScene) {
          currentSceneIdx = 3;
      } else {
          if (ResourceManager::getTexture("floorDiff") == ResourceManager::getTexture("grassDiff")) {
              currentSceneIdx = 2;
          } else {
              currentSceneIdx = 0;
          }
      }
      
      const char* sceneNames[] = { "Water Demo", "Collision Demo", "World Demo", "Buoyancy Demo" };
      int selectedIdx = currentSceneIdx;
      if (ImGui::Combo("##ActiveScene", &selectedIdx, sceneNames, IM_ARRAYSIZE(sceneNames))) {
          physicsSystem->reset();
          scene->entities.clear(); selectedEntityIndex = -1;
          if (selectedIdx == 0) {
              loadDefaultScene();
              isCollisionDemo = false;
              isBuoyancyScene = false;
          } else if (selectedIdx == 1) {
              loadCollisionDemoScene();
              isCollisionDemo = true;
              isBuoyancyScene = false;
          } else if (selectedIdx == 2) {
              loadWorldScene();
              isCollisionDemo = false;
              isBuoyancyScene = false;
          } else if (selectedIdx == 3) {
              loadBuoyancyScene(3);
              isCollisionDemo = false;
              isBuoyancyScene = true;
          }
      }
      
      if (isBuoyancyScene) {
          ImGui::Separator();
          ImGui::Text("Buoyancy Simulation Scenarios");
          const char* scenarioNames[] = { 
              "1: Sphere (r=0.5m, m=157kg)", 
              "2: Wooden Slab (1x1x0.3m, tilt)", 
              "3: Open Box (1.2x1.2x0.6m)", 
              "4: Sandbox (All Side-by-Side)" 
          };
          int activeScenario = currentScenario;
          if (ImGui::Combo("##BuoyancyScenario", &activeScenario, scenarioNames, IM_ARRAYSIZE(scenarioNames))) {
              loadBuoyancyScene(activeScenario);
          }
          ImGui::Checkbox("Debug Overlay (F1 Key)", &debugBuoyancy);
          ImGui::Separator();
      }
      ImGui::Checkbox("Normal Map", &useNormalMap); ImGui::SameLine(); ImGui::Checkbox("Light 2 Moving", &light2Moving);
      ImGui::Checkbox("Water Waves (F4)", &waterWavesEnabled);
      const char* waterDebugNames[] = {
          "None", "Alpha", "Scene Depth", "Refraction", "Fresnel", "Ripple Height", "Visual Height", "Physics Height",
          "Dry Cavity Mask", "Internal Box Water", "Invalid Global Intersection", "Raw Water Height",
          "Disable Detail Normal", "Disable Ripple Normal", "Disable Gerstner Normal", "Reconstructed Ripple Normals",
          "Final Normal", "NdotL", "NdotH", "Specular", "Reflection", "Final Lighting"
      };
      ImGui::Combo("Water Debug (F5)", &waterDebugMode, waterDebugNames, IM_ARRAYSIZE(waterDebugNames));
      if (ImGui::CollapsingHeader("Water Ripple Tuning", ImGuiTreeNodeFlags_DefaultOpen)) {
          RippleTuning& water = RippleSystem::instance().tuning();
          ImGui::SliderFloat("rippleAmplitude", &water.rippleAmplitude, 0.2f, 1.6f, "%.2f");
          ImGui::SliderFloat("visualRippleScale", &water.visualRippleScale, 0.0f, 2.5f, "%.2f");
          ImGui::SliderFloat("physicsRippleScale", &water.physicsRippleScale, 0.0f, 0.3f, "%.2f");
          ImGui::SliderFloat("rippleDamping", &water.rippleDamping, 0.950f, 0.999f, "%.3f");
          ImGui::SliderFloat("ripplePropagationSpeed", &water.ripplePropagationSpeed, 0.5f, 3.2f, "%.2f");
          ImGui::SliderFloat("maxRippleHeight", &water.maxRippleHeight, 0.025f, 0.080f, "%.3fm");
          ImGui::SliderFloat("rippleNoiseThreshold", &water.rippleNoiseThreshold, 0.0000f, 0.0050f, "%.4fm");
          ImGui::SliderFloat("reflectionStrength", &water.reflectionStrength, 0.4f, 3.0f, "%.2f");
          ImGui::SliderFloat("FresnelStrength", &water.fresnelStrength, 0.4f, 2.5f, "%.2f");
          ImGui::SliderFloat("specularStrength", &water.specularStrength, 0.0f, 5.0f, "%.2f");
          ImGui::SliderFloat("shininess", &water.shininess, 16.0f, 256.0f, "%.0f");
          ImGui::SliderFloat("waterRoughness", &water.waterRoughness, 0.02f, 0.65f, "%.2f");
          ImGui::SliderFloat("normalStrength", &water.normalStrength, 0.0f, 2.0f, "%.2f");
          ImGui::SliderFloat("crestHighlightStrength", &water.crestHighlightStrength, 0.0f, 3.0f, "%.2f");
          ImGui::ColorEdit3("skyReflectionColor", glm::value_ptr(water.skyReflectionColor));
          ImGui::SliderFloat("waveSteepness", &water.waveSteepness, 0.3f, 1.8f, "%.2f");
          ImGui::SliderFloat("rippleNormalStrength", &water.rippleNormalStrength, 0.2f, 2.0f, "%.2f");
          ImGui::SliderFloat("waterNormalStrength", &water.waterNormalStrength, 0.2f, 2.5f, "%.2f");
      }
      ImGui::Separator();
      ImGui::Text("Advanced Global"); ImGui::SliderFloat("Tess Level", &tessLevel, 1, 64); ImGui::SliderFloat("Explosion", &explosionFactor, 0, 1);
      ImGui::SliderFloat("Shadow Bias", &shadowBias, 0.0001f, 0.05f, "%.4f");
      ImGui::SliderFloat("PCF Radius", &pcfRadius, 0.0f, 5.0f, "%.1f");
      ImGui::Text("Tip: Ctrl+Click sliders to type numbers manually");
      ImGui::Separator();
      ImGui::Text("Particle System"); ImGui::SliderFloat("P Count", &pCount, 1, 256); ImGui::SliderFloat("P Spread", &pSpread, 0.1f, 5.0f); ImGui::SliderFloat("P Size", &pSize, 0.01f, 0.5f);
      ImGui::End(); }

    {
      ImGui::Begin("Controls & Instructions");
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Camera Navigation");
      ImGui::BulletText("TAB     : Toggle mouse cursor / look-around");
      ImGui::BulletText("W/A/S/D : Move camera horizontally");
      ImGui::BulletText("SPACE   : Move camera vertically upward");
      ImGui::BulletText("SHIFT   : Move camera vertically downward");
      ImGui::BulletText("CTRL    : Hold to sprint camera speed");
      
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Diagnostics & Overlay");
      ImGui::BulletText("F1      : Toggle Buoyancy Debug Overlay");
      ImGui::BulletText("F2      : Toggle GPU Pass Timings Overlay");
      ImGui::BulletText("F3      : Cycle G-Buffer visualization target");
      ImGui::BulletText("F4      : Toggle Water Waves");
      ImGui::BulletText("F5      : Cycle Water debug target");
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "          (None -> Albedo -> Normals -> Roughness -> Metallic -> Depth)");
      
      if (isBuoyancyScene) {
          ImGui::Separator();
          ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "Buoyancy Scenarios");
          ImGui::BulletText("1       : Load Scenario 1 (Floating Sphere)");
          ImGui::BulletText("2       : Load Scenario 2 (Wooden Slab Tilt/Rotation)");
          ImGui::BulletText("3       : Load Scenario 3 (Open-top Flooding Box)");
          ImGui::BulletText("4       : Load Scenario 4 (Sandbox Side-by-Side)");
          
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Interactive Testing Tasks:");
          ImGui::BulletText("Sphere  : Select 'Hollow Sphere', drag 'Position Y' slider in Inspector");
          ImGui::BulletText("          down to bottom, release to watch damped oscillation & bobbing.");
          ImGui::BulletText("Slab    : Rotate wooden slab, release to watch stability center restore it.");
          ImGui::BulletText("Box     : Select 'Box Cargo' in list. Slide 'Local Offset X/Z' or raise");
          ImGui::BulletText("          'Mass' to watch dynamic tilting, water flooding, and capsizing!");
      }
      
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Advanced UI Tip");
      ImGui::Text("Hold Ctrl + Click any slider to type precise values!");
      ImGui::End();
    }

    if (isCollisionDemo) {
      ImGui::Begin("Collision Controls"); 
      ImGui::SliderInt("Sphere Count (k)", &kSpheresCount, 1, 300);
      ImGui::Checkbox("Use Spatial Grid", &useSpatialGrid);
      ImGui::Text("Collision Checks: %d", g_collisionChecks);
      if (ImGui::Button("Shoot Spheres")) {
          for(int i=0; i<kSpheresCount; i++) {
              float rx = ((rand() % 2000) / 100.0f) - 10.0f;
              float rz = ((rand() % 2000) / 100.0f) - 10.0f;
              Entity s("DynamicSphere", SPHERE, glm::vec3(rx, 15.0f + (rand()%5), rz), glm::vec3(0.5f));
              s.originalColor = glm::vec3(0.5f);
              s.scale = glm::vec3(0.5f); // diameter=1m
              s.radius = 0.5f;
              s.mass = 1.0f;
              s.roughness = 0.3f; s.metallic = 0.1f;
              s.localBounds = AABB(glm::vec3(-1.0f), glm::vec3(1.0f));
              s.velocity = glm::vec3(((rand()%200)/100.0f)-1.0f, -5.0f - (rand()%5), ((rand()%200)/100.0f)-1.0f);
              scene->addEntity(s);
          }
      }
      if (ImGui::Button("Clear Spheres")) {
          auto newEnd = std::remove_if(scene->entities.begin(), scene->entities.end(), [](const Entity& e){ return e.name == "DynamicSphere"; });
          scene->entities.erase(newEnd, scene->entities.end());
      }
      ImGui::End(); }

    if (showProfilingOverlay) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("GPU Profiler Overlay", nullptr, window_flags)) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "GPU PROFILE METRICS (F2)");
            ImGui::Separator();
            
            ImGui::Text("Shadow Pass:    %6.2f ms", renderer->timeShadow);
            ImGui::Text("Geometry Pass:  %6.2f ms", renderer->timeGeometry);
            ImGui::Text("IBL / Deferred: %6.2f ms", renderer->timeIBL);
            ImGui::Text("Water & Glass:  %6.2f ms", renderer->timeWater);
            ImGui::Text("Post & Debug:   %6.2f ms", renderer->timePost);
            
            ImGui::Separator();
            float totalGpu = renderer->timeShadow + renderer->timeGeometry + renderer->timeIBL + renderer->timeWater + renderer->timePost;
            ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "Total GPU Time: %6.2f ms", totalGpu);
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "CPU Frame Time: %6.2f ms (%.1f FPS)", deltaTime * 1000.0f, 1.0f / deltaTime);

            if (gbufferVisualisationMode > 0) {
                const char* modes[] = { "None", "Albedo", "Normals", "Roughness", "Metallic", "Depth" };
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "G-Buffer View:  %s (F3)", modes[gbufferVisualisationMode]);
            }
            ImGui::End();
        }
    }

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
        
        if (isCollisionDemo) {
            physicsSystem->update(scene, deltaTime, useSpatialGrid, waterWavesEnabled);
        } else if (isBuoyancyScene) {
            physicsSystem->update(scene, deltaTime, false, waterWavesEnabled);
        }

        renderer->renderScene(scene, useNormalMap, tessLevel, explosionFactor, pSpread, pSize, pCount, shadowBias, pcfRadius, isCollisionDemo, debugBuoyancy, gbufferVisualisationMode, waterWavesEnabled, waterDebugMode);
        
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

unsigned int Application::loadCubemap(std::vector<std::string> faces) {
    unsigned int tid; glGenTextures(1, &tid); glBindTexture(GL_TEXTURE_CUBE_MAP, tid); stbi_set_flip_vertically_on_load(false);
    for(int i=0; i<6; i++){ int w,h,c; std::string facePath = AssetPath::resolve(faces[i]).string(); unsigned char *d=stbi_load(facePath.c_str(), &w, &h, &c, 0); if(d) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, d); stbi_image_free(d); }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    stbi_set_flip_vertically_on_load(true); return tid;
}
