#ifndef ENTITY_H
#define ENTITY_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include "Collider.h"

enum EntityType { CUBE, SPHERE, ICOSAHEDRON, ADV_SPHERE, FLOOR, PARTICLE, WATER, MODEL };

class Model; // 前向宣告

struct Entity {
    std::string name;
    EntityType type;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec3 color;
    glm::vec3 originalColor;
    bool visible = true;
    
    // 模型資源
    Model* model = nullptr;
    
    // PBR 材質屬性
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ambient = 0.1f;
    float reflectivity = 0.0f;

    // 光源屬性
    bool isLight = false;
    glm::vec3 lightColor = glm::vec3(1.0f);
    float lightIntensity = 1.0f;

    // 動態紋理
    bool dynamicTexture = false;
    float texSpeed = 0.1f;
    
    // 碰撞感測與物理加速
    bool hasCollision = true;
    AABB localBounds;
    glm::vec3 velocity = glm::vec3(0.0f);
    float radius = 0.5f;
    float mass = 1.0f;

    Entity(std::string n, EntityType t, glm::vec3 pos = glm::vec3(0.0f), glm::vec3 col = glm::vec3(1.0f))
        : name(n), type(t), position(pos), rotation(0.0f), scale(1.0f), color(col), originalColor(col) {}
        
    AABB getGlobalBounds() const {
        glm::vec3 globalMin = position + localBounds.minExtents * scale;
        glm::vec3 globalMax = position + localBounds.maxExtents * scale;
        return AABB(glm::min(globalMin, globalMax), glm::max(globalMin, globalMax));
    }

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, scale);
        return model;
    }
};

#endif
