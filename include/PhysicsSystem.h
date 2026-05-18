#ifndef PHYSICS_SYSTEM_H
#define PHYSICS_SYSTEM_H

#include "Scene.h"
#include <vector>

extern int g_collisionChecks;

struct Cell {
    std::vector<int> staticEntities;
    std::vector<int> dynamicEntities;
};

class PhysicsSystem {
public:
    PhysicsSystem();
    void buildStaticGrid(Scene* scene);
    void update(Scene* scene, float deltaTime, bool useGrid, bool waterWavesEnabled = true);
    void reset();

private:
    Cell grid[7][7][7];
    bool gridBuilt = false;

    void updateExhaustive(Scene* scene, float deltaTime);
    void updateGrid(Scene* scene, float deltaTime);
    struct WaterQuery {
        glm::vec4 samplePosition;
        glm::vec4 waterPosition;
        glm::vec4 waterNormal;
        glm::vec4 waterData;
    };

    void updateBuoyancy(Entity& e, float deltaTime, const std::vector<WaterQuery>& waterQueries, int& queryIndex, float cargoMass = 0.0f, glm::vec3 cargoOffset = glm::vec3(0.0f));
    
    // Physics Math
    void resolveCollisionSphereAABB(Entity* currSphere, Entity* box);
    void resolveCollisionSphereSphere(Entity* s1, Entity* s2);
    void resolveBuoyantBodyCollisions(Scene* scene);
    void resolveCollisionBuoyantBodies(Entity* a, Entity* b);
    
    // Limits
    glm::ivec3 getGridCell(glm::vec3 pos) {
        int cx = (int)glm::clamp((pos.x + 10.5f) / 3.0f, 0.0f, 6.0f);
        int cy = (int)glm::clamp(pos.y / 3.0f, 0.0f, 6.0f);
        int cz = (int)glm::clamp((pos.z + 10.5f) / 3.0f, 0.0f, 6.0f);
        return glm::ivec3(cx, cy, cz);
    }
    
    AABB getGridAABB(int cx, int cy, int cz) {
        glm::vec3 minP = glm::vec3(-10.5f + cx*3.0f, cy*3.0f, -10.5f + cz*3.0f);
        return AABB(minP, minP + glm::vec3(3.0f));
    }
};

#endif
