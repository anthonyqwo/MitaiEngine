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
#include "IBLBaker.h"

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
    delete renderer;
    delete physicsSystem;
    ResourceManager::clear();
    delete helmetModel;
    delete pmxModel;
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
    ResourceManager::loadShader("shaders/skinning_v.glsl", "shaders/gbuffer_f.glsl", nullptr, nullptr, nullptr, "skinning");
    ResourceManager::loadShader("shaders/skinning_v.glsl", "shaders/shadow_f.glsl", nullptr, nullptr, nullptr, "skinning_shadow");
    ResourceManager::loadShader("shaders/skinning_v.glsl", "shaders/point_shadow_f.glsl", "shaders/point_shadow_g.glsl", nullptr, nullptr, "skinning_pointShadow");
    ResourceManager::loadShader("shaders/adv_v.glsl", "shaders/adv_gbuffer_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl", "advGbuffer");
    ResourceManager::loadShader("shaders/deferred_lighting_v.glsl", "shaders/deferred_lighting_f.glsl", nullptr, nullptr, nullptr, "deferred_lighting");
    
    ResourceManager::loadShader("shaders/vertex.glsl", "shaders/forward_water_f.glsl", nullptr, nullptr, nullptr, "forward_water");
    ResourceManager::loadShader("shaders/vertex.glsl", "shaders/unlit_f.glsl", nullptr, nullptr, nullptr, "unlit");
    
    ResourceManager::loadShader("shaders/skybox_v.glsl", "shaders/skybox_f.glsl", nullptr, nullptr, nullptr, "skybox");
    ResourceManager::loadShader("shaders/shadow_v.glsl", "shaders/shadow_f.glsl", nullptr, nullptr, nullptr, "shadow");
    ResourceManager::loadShader("shaders/point_shadow_v.glsl", "shaders/point_shadow_f.glsl", "shaders/point_shadow_g.glsl", nullptr, nullptr, "pointShadow");
    ResourceManager::loadShader("shaders/particle_v.glsl", "shaders/particle_f.glsl", "shaders/particle_g.glsl", "shaders/particle_tc.glsl", "shaders/particle_te.glsl", "particle");
    ResourceManager::loadShader("shaders/adv_v.glsl", "shaders/shadow_f.glsl", "shaders/adv_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl", "advShadow");
    ResourceManager::loadShader("shaders/adv_v.glsl", "shaders/point_shadow_f.glsl", "shaders/adv_point_shadow_g.glsl", "shaders/adv_tc.glsl", "shaders/adv_te.glsl", "advPointShadow");

    ResourceManager::loadTexture("assets/container.jpg", "texDiff");
    ResourceManager::loadTexture("assets/container_specular.png", "texSpec");
    ResourceManager::loadTexture("assets/container_normal.png", "texNorm");
    ResourceManager::loadTexture("assets/water_normal.jpg", "waterNorm");
    ResourceManager::loadTexture("assets/wall.jpg", "floorDiff");
    ResourceManager::loadTexture("assets/wall_normal.jpg", "floorNorm");
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
    pmxModel = new Model("assets/models/butterfly.pmx");
    
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
    scene->camera.Position = glm::vec3(0.0f, 2.0f, 10.0f);
    scene->camera.Pitch = 0.0f;
    scene->camera.Yaw = -90.0f;
    scene->camera.ProcessMouseMovement(0, 0);

    Entity butterflyEnt("Butterfly PMX", MODEL, glm::vec3(0, 0, 0), glm::vec3(1));
    butterflyEnt.model = pmxModel;
    butterflyEnt.scale = glm::vec3(0.12f);
    butterflyEnt.metallic = 0.5f; butterflyEnt.roughness = 0.5f; butterflyEnt.ambient = 1.0f; butterflyEnt.reflectivity = 0.3f;
    butterflyEnt.localBounds = pmxModel->localBounds;
    scene->addEntity(butterflyEnt);

    Entity helmetEnt("Damaged Helmet", MODEL, glm::vec3(3.0f, 1.0f, 0), glm::vec3(1));
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

void Application::renderImGui() {
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
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
      if (ImGui::Button("Scene: Water Demo")) {
          scene->entities.clear(); selectedEntityIndex = -1;
          loadDefaultScene();
          isCollisionDemo = false;
      }
      ImGui::SameLine();
      if (ImGui::Button("Scene: Collision Demo")) {
          scene->entities.clear(); selectedEntityIndex = -1;
          loadCollisionDemoScene();
          isCollisionDemo = true;
      }
      ImGui::Separator();
      ImGui::Checkbox("Normal Map", &useNormalMap); ImGui::Checkbox("Light 2 Moving", &light2Moving); ImGui::Separator();
      ImGui::Text("Advanced Global"); ImGui::SliderFloat("Tess Level", &tessLevel, 1, 64); ImGui::SliderFloat("Explosion", &explosionFactor, 0, 1);
      ImGui::Separator();
      ImGui::Text("Particle System"); ImGui::SliderFloat("P Count", &pCount, 1, 256); ImGui::SliderFloat("P Spread", &pSpread, 0.1f, 5.0f); ImGui::SliderFloat("P Size", &pSize, 0.01f, 0.5f);
      ImGui::End(); }

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
            physicsSystem->update(scene, deltaTime, useSpatialGrid);
        }

        renderer->renderScene(scene, useNormalMap, tessLevel, explosionFactor, pSpread, pSize, pCount, isCollisionDemo);
        
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
    for(int i=0; i<6; i++){ int w,h,c; unsigned char *d=stbi_load(faces[i].c_str(), &w, &h, &c, 0); if(d) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, d); stbi_image_free(d); }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    stbi_set_flip_vertically_on_load(true); return tid;
}
