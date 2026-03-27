#ifndef COLLIDER_H
#define COLLIDER_H

#include <glm/glm.hpp>
#include <algorithm>

struct AABB {
    glm::vec3 minExtents;
    glm::vec3 maxExtents;

    AABB() : minExtents(glm::vec3(0.0f)), maxExtents(glm::vec3(0.0f)) {}
    AABB(const glm::vec3& minE, const glm::vec3& maxE) : minExtents(minE), maxExtents(maxE) {}
    
    // Check if this AABB intersects with another AABB
    bool intersects(const AABB& other) const {
        return (minExtents.x <= other.maxExtents.x && maxExtents.x >= other.minExtents.x) &&
               (minExtents.y <= other.maxExtents.y && maxExtents.y >= other.minExtents.y) &&
               (minExtents.z <= other.maxExtents.z && maxExtents.z >= other.minExtents.z);
    }
};

#endif
