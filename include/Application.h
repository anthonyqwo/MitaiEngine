#ifndef APPLICATION_H
#define APPLICATION_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>


#include "Model.h"
#include "PhysicsSystem.h"
#include "Renderer.h"
#include "Scene.h"


class Application {
public:
  Application();
  ~Application();

  bool init();
  void loadDefaultScene();
  void loadCollisionDemoScene();
  void loadWorldScene();
  void loadBuoyancyScene(int scenario);
  void run();

private:
  void processInput();
  void renderImGui();
  void setupResources();

  GLFWwindow *window;
  Scene *scene;
  Renderer *renderer;
  PhysicsSystem *physicsSystem;

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
  float shadowBias;
  float pcfRadius;
  int selectedEntityIndex;

  bool isCollisionDemo = false;
  bool isBuoyancyScene = false;
  bool debugBuoyancy = false;
  bool waterWavesEnabled = true;
  int currentScenario = 0;
  bool showProfilingOverlay = false;
  int gbufferVisualisationMode = 0; // 0=None, 1=Albedo, 2=Normals, 3=Roughness, 4=Metallic, 5=Depth
  int waterDebugMode = 0; // 0=None, 5=Ripple, 11=Raw Height, 12-15=normal isolation modes

  // Collision Physics Simulation Config
  bool useSpatialGrid = true;
  int kSpheresCount = 50;

  Model *helmetModel;
  Model *houseModel;
  Model *treeModel;

  static Application *s_instance;
  static void framebuffer_size_callback(GLFWwindow *window, int width,
                                        int height);
  static void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);

  // IBL Helpers
  unsigned int loadCubemap(std::vector<std::string> faces);
};

#endif
