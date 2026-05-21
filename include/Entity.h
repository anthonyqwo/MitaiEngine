#ifndef ENTITY_H
#define ENTITY_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <glm/gtc/quaternion.hpp>
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
    std::string albedoTexture;
    std::string normalTexture;
    std::string metallicTexture;
    std::string roughnessTexture;
    std::string aoTexture;

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

    // 浮力模擬屬性 (Buoyancy Simulation Properties)
    bool isBuoyant = false;
    int buoyancyType = 0; // 0 = SPHERE, 1 = SLAB, 2 = OPEN_BOX
    glm::vec3 angularVelocity = glm::vec3(0.0f);
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 buoyancyCenter = glm::vec3(0.0f);
    glm::vec3 buoyancyForce = glm::vec3(0.0f);
    glm::vec3 waterDragForce = glm::vec3(0.0f);
    glm::vec3 dampingForce = glm::vec3(0.0f);
    glm::vec3 torqueDebug = glm::vec3(0.0f);
    glm::vec3 angularVelocityDebug = glm::vec3(0.0f);
    glm::vec3 waterSurfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    std::vector<glm::vec3> debugBuoyancySamples;
    std::vector<glm::vec3> debugRawWaterSurfacePoints;
    std::vector<glm::vec3> debugWaterSurfacePoints;
    std::vector<glm::vec3> debugWaterSurfaceNormals;
    std::vector<float> debugWaterSubmergedDepths;
    std::vector<glm::vec3> debugRippleInjectionPoints;
    std::vector<float> debugRippleInjectionRadii;
    std::vector<float> debugRippleInjectionPressures;
    std::vector<float> debugRippleInjectionStrengths;
    std::vector<glm::vec3> debugWallContactPoints;
    std::vector<glm::vec3> debugWallContactNormals;
    std::vector<glm::vec3> debugWallCorrectionVectors;
    std::vector<glm::vec3> debugWallImpulseVectors;
    std::vector<glm::vec3> debugWallTangentialVelocities;
    std::vector<float> debugWallPenetrationDepths;
    std::vector<float> debugWallNormalImpulses;
    std::vector<float> debugWallFrictionImpulses;
    float submergedFraction = 0.0f;
    float floodLevel = 0.0f;
    float dryTimer = 0.0f;

    // Cargo-load extension
    bool isCargo = false;
    glm::vec3 cargoLocalOffset = glm::vec3(0.0f);

    // Grabbing state
    bool isGrabbed = false;
    glm::vec3 localGrabOffset = glm::vec3(0.0f);
    glm::vec3 targetGrabWorld = glm::vec3(0.0f);

    // AI Engine metadata extension (HW9)
    bool isAI = false;
    int aiRole = 0;      // 0=Prey, 1=Predator
    int aiSpecies = 0;   // 0=Green, 1=Blue, 2=Predator1, 3=Predator2
    int aiState = 0;     // Wander, Flee, Search, Chase
    float aiVisionRange = 0.0f;
    float aiVisionFOV = 0.0f;
    std::vector<glm::vec3> aiPath; // copy for debug renderer

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
        if (isBuoyant) {
            model = model * glm::mat4_cast(orientation);
        } else {
            model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
            model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
            model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        }
        model = glm::scale(model, scale);
        return model;
    }
};

#endif
