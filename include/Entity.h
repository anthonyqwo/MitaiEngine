#ifndef ENTITY_H
#define ENTITY_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

enum EntityType { CUBE, SPHERE, ICOSAHEDRON, ADV_SPHERE, FLOOR, PARTICLE, WATER };

struct Entity {
    std::string name;
    EntityType type;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec3 color;
    bool visible = true;
    
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

    Entity(std::string n, EntityType t, glm::vec3 pos = glm::vec3(0.0f), glm::vec3 col = glm::vec3(1.0f))
        : name(n), type(t), position(pos), rotation(0.0f), scale(1.0f), color(col) {}

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
