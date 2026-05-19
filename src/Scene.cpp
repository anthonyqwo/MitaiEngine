#include "Scene.h"
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
    glm::vec3 waterCenter(0.0f);
    bool hasWater = false;
    for (const auto& e : entities) {
        if (e.visible && e.type == WATER) {
            waterCenter = e.position;
            hasWater = true;
            break;
        }
    }

    for (auto& e : entities) {
        if (e.name == "Dynamic Cube") e.rotation.y += 30 * deltaTime;
        if (e.name == "Advanced Sphere") e.rotation.y += 20 * deltaTime;
        // 注意原本 main.cpp 中是 e.name == "Icosahedron"，但插入時名為 "Advanced Sphere"
        // 我們改為比對 Advanced Sphere 來兼容 Imgui 加的邏輯或者是實體本身
        if (light2Moving && e.name == "Point Light") {
            e.position = glm::vec3(sin(currentTime)*3, 2, cos(currentTime)*3);
        }
        if (e.name == "Water Highlight Light") {
            glm::vec3 center = hasWater ? waterCenter : glm::vec3(0.0f);
            float radius = hasWater ? 2.65f : 2.6f;
            float height = hasWater ? 1.85f : 3.2f;
            float t = currentTime * 1.85f;
            e.position = center + glm::vec3(cos(t) * radius, height + sin(currentTime * 2.4f) * 0.22f, sin(t) * radius);
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
