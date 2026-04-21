#include "PhysicsSystem.h"
#include <glm/glm.hpp>
#include <iostream>

int g_collisionChecks = 0;

void PhysicsSystem::reset() {
    gridBuilt = false;
    g_collisionChecks = 0;
}

void PhysicsSystem::buildStaticGrid(Scene* scene) {
    if (gridBuilt) return;
    for (int x=0; x<7; x++) {
        for (int y=0; y<7; y++) {
            for (int z=0; z<7; z++) {
                grid[x][y][z].staticEntities.clear();
            }
        }
    }
    for (int i = 0; i < scene->entities.size(); i++) {
        auto& entity = scene->entities[i];
        if (entity.name == "DynamicSphere") continue;
        if (!entity.hasCollision || entity.mass > 0.0f) continue;
        
        AABB objAABB = entity.getGlobalBounds();
        for (int x=0; x<7; x++) {
            for (int y=0; y<7; y++) {
                for (int z=0; z<7; z++) {
                    AABB cellAABB = getGridAABB(x, y, z);
                    if (objAABB.intersects(cellAABB)) {
                        grid[x][y][z].staticEntities.push_back(i);
                    }
                }
            }
        }
    }
    gridBuilt = true;
}

PhysicsSystem::PhysicsSystem() {}

void PhysicsSystem::update(Scene* scene, float deltaTime, bool useGrid) {
    g_collisionChecks = 0;
    
    // First, Euler integration for velocities
    for (auto& e : scene->entities) {
        if (e.name == "DynamicSphere") {
            e.velocity.y -= 9.81f * deltaTime; // gravity
            e.position += e.velocity * deltaTime;
        }
    }
    
    if (useGrid) {
        if (!gridBuilt) buildStaticGrid(scene);
        updateGrid(scene, deltaTime);
    } else {
        updateExhaustive(scene, deltaTime);
    }
}

void PhysicsSystem::resolveCollisionSphereAABB(Entity* sphere, Entity* box) {
    AABB bAABB = box->getGlobalBounds();
    
    glm::vec3 closestP = glm::max(bAABB.minExtents, glm::min(sphere->position, bAABB.maxExtents));
    float dist = glm::length(sphere->position - closestP);
    
    if (dist < sphere->radius) {
        // Intersect
        glm::vec3 N = glm::normalize(sphere->position - closestP);
        if (glm::length(sphere->position - closestP) == 0.0f) N = glm::vec3(0, 1, 0); // fallback inner
        
        // Push out
        sphere->position += N * (sphere->radius - dist);
        
        // Reflect velocity
        float velAlongNormal = glm::dot(sphere->velocity, N);
        if (velAlongNormal < 0) {
            float restitution = 0.8f; // bouncy
            sphere->velocity -= (1.0f + restitution) * velAlongNormal * N;
        }
        
        // Color change
        sphere->color = box->originalColor;
    }
}

void PhysicsSystem::resolveCollisionSphereSphere(Entity* s1, Entity* s2) {
    if (s1 == s2) return;
    
    glm::vec3 diff = s1->position - s2->position;
    float dist = glm::length(diff);
    float rSum = s1->radius + s2->radius;
    
    if (dist < rSum && dist > 0.0001f) {
        glm::vec3 N = diff / dist;
        float penetration = rSum - dist;
        
        // Push out (equal mass assumption)
        s1->position += N * (penetration * 0.5f);
        s2->position -= N * (penetration * 0.5f);
        
        glm::vec3 relVel = s1->velocity - s2->velocity;
        float velAlongNormal = glm::dot(relVel, N);
        
        if (velAlongNormal < 0) {
            float restitution = 0.9f; 
            float impulse = -(1.0f + restitution) * velAlongNormal / 2.0f; // 2.0 = 1/m1 + 1/m2
            glm::vec3 impVec = impulse * N;
            
            s1->velocity += impVec;
            s2->velocity -= impVec;
            
            // Revert to grey
            s1->color = s1->originalColor;
            s2->color = s2->originalColor;
        }
    }
}

void PhysicsSystem::updateExhaustive(Scene* scene, float deltaTime) {
    std::vector<Entity*> dynamicSpheres;
    std::vector<Entity*> staticObjects;
    
    for (auto& e : scene->entities) {
        if (e.name == "DynamicSphere") dynamicSpheres.push_back(&e);
        else if (e.hasCollision) staticObjects.push_back(&e);
    }
    
    for (int i=0; i<dynamicSpheres.size(); i++) {
        Entity* sphere = dynamicSpheres[i];
        
        // Check Static
        for (int j=0; j<staticObjects.size(); j++) {
            g_collisionChecks++;
            resolveCollisionSphereAABB(sphere, staticObjects[j]);
        }
        
        // Check Spheres (i < j to avoid double checking)
        for (int j=i+1; j<dynamicSpheres.size(); j++) {
            g_collisionChecks++;
            resolveCollisionSphereSphere(sphere, dynamicSpheres[j]);
        }
    }
}

void PhysicsSystem::updateGrid(Scene* scene, float deltaTime) {
    // 1. Clear ONLY dynamic grids (statics are precalculated indices)
    for (int x=0; x<7; x++) {
        for (int y=0; y<7; y++) {
            for (int z=0; z<7; z++) {
                grid[x][y][z].dynamicEntities.clear();
            }
        }
    }
    
    std::vector<int> dynamicSpheres;
    
    // 2. Classify dynamic entities by index
    for (int i = 0; i < scene->entities.size(); i++) {
        auto& e = scene->entities[i];
        if (!e.hasCollision) continue;
        
        if (e.name == "DynamicSphere") {
            dynamicSpheres.push_back(i);
            glm::ivec3 cell = getGridCell(e.position);
            grid[cell.x][cell.y][cell.z].dynamicEntities.push_back(i);
        }
    }
    
    // 3. Resolve using grid via index loops
    for (int sphereIdx : dynamicSpheres) {
        Entity* sphere = &scene->entities[sphereIdx];
        glm::ivec3 cell = getGridCell(sphere->position);
        
        std::vector<int> checkedStatics; // Prevent duplicate checking across adjacent cells
        
        int r = 1; // adjacent extent
        for (int nx = std::max(0, cell.x - r); nx <= std::min(6, cell.x + r); nx++) {
            for (int ny = std::max(0, cell.y - r); ny <= std::min(6, cell.y + r); ny++) {
                for (int nz = std::max(0, cell.z - r); nz <= std::min(6, cell.z + r); nz++) {
                    
                    for (int staticIdx : grid[nx][ny][nz].staticEntities) {
                        bool alreadyChecked = false;
                        for (int k : checkedStatics) {
                            if (k == staticIdx) { alreadyChecked = true; break;}
                        }
                        if (!alreadyChecked) {
                            g_collisionChecks++;
                            resolveCollisionSphereAABB(sphere, &scene->entities[staticIdx]);
                            checkedStatics.push_back(staticIdx);
                        }
                    }
                    
                    for (int otherDynIdx : grid[nx][ny][nz].dynamicEntities) {
                        if (sphereIdx != otherDynIdx && sphereIdx < otherDynIdx) { 
                            g_collisionChecks++;
                            resolveCollisionSphereSphere(sphere, &scene->entities[otherDynIdx]);
                        }
                    }
                }
            }
        }
    }
}
