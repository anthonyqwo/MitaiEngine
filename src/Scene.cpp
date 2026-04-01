#include "Scene.h"
#include "Model.h"
#include <iostream>

Scene::Scene() : camera(glm::vec3(0.0f, 2.0f, 8.0f)) {
    skybox = 0;
    irradianceMap = 0;
    prefilterMap = 0;
    brdfLUT = 0;
}

void Scene::addEntity(const Entity& e) {
    entities.push_back(e);
}

void Scene::update(float deltaTime, float currentTime, bool light2Moving) {
    for (auto& e : entities) {
        if (e.name == "Dynamic Cube") e.rotation.y += 30 * deltaTime;
        if (e.name == "Advanced Sphere") e.rotation.y += 20 * deltaTime;
        
        if (e.type == MODEL && e.model) {
            if (!e.model->GetBoneInfoMap().empty()) {
                e.finalBoneMatrices.resize(e.model->GetBoneCount());
                e.model->UpdateBoneMatrices(e.model->GetScene()->mRootNode, glm::mat4(1.0f), e.finalBoneMatrices);
            }
        }

        if (light2Moving && e.name == "Point Light") {
            e.position = glm::vec3(sin(currentTime)*3, 2, cos(currentTime)*3);
        }
    }
}

void Scene::processCollisions(glm::vec3 movement) {
    auto checkCollisions = [&]() -> bool {
        AABB camAABB(camera.Position - glm::vec3(camera.collisionRadius), camera.Position + glm::vec3(camera.collisionRadius));
        for (const auto& e : entities) {
            if (e.hasCollision) {
                if (camAABB.intersects(e.getGlobalBounds())) {
                    return true;
                }
            }
        }
        return false;
    };

    camera.Position.x += movement.x;
    if (checkCollisions()) camera.Position.x -= movement.x; 
    
    camera.Position.y += movement.y;
    if (checkCollisions()) camera.Position.y -= movement.y; 
    
    camera.Position.z += movement.z;
    if (checkCollisions()) camera.Position.z -= movement.z; 
}
