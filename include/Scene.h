#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "Entity.h"
#include "Camera.h"

class Scene {
public:
    std::vector<Entity> entities;
    Camera camera;
    
    // IBL textures
    unsigned int skybox;
    unsigned int irradianceMap;
    unsigned int prefilterMap;
    unsigned int brdfLUT;

    Scene();
    void addEntity(const Entity& e);
    void update(float deltaTime, float currentTime, bool light2Moving);
    void processCollisions(glm::vec3 movement);
};

#endif
