#ifndef NAVIGATION_GRID_H
#define NAVIGATION_GRID_H

#include <vector>
#include <glm/glm.hpp>
#include "Collider.h"

struct GridCell {
    int x, z;
    bool walkable;
};

struct AStarNode {
    int x, z;
    float gCost;
    float hCost;
    float fCost;
    int parentX;
    int parentZ;

    AStarNode(int px = 0, int pz = 0, float g = 0.0f, float h = 0.0f, int parX = -1, int parZ = -1)
        : x(px), z(pz), gCost(g), hCost(h), fCost(g + h), parentX(parX), parentZ(parZ) {}

    bool operator>(const AStarNode& other) const {
        return fCost > other.fCost;
    }
};

class NavigationGrid {
public:
    NavigationGrid();
    void initialize(float minX, float maxX, float minZ, float maxZ, float cellSize);
    void buildObstacles(const std::vector<class Entity>& entities);
    
    bool isWalkable(float x, float z) const;
    bool isWalkableCell(int cx, int cz) const;
    
    glm::ivec2 worldToCell(glm::vec3 worldPos) const;
    glm::vec3 cellToWorld(int cx, int cz) const;
    
    std::vector<glm::vec3> findPath(glm::vec3 start, glm::vec3 end) const;
    
    glm::vec3 getRandomWalkablePosition() const;
    
    float getCellSize() const { return m_cellSize; }
    int getWidth() const { return m_width; }
    int getDepth() const { return m_depth; }
    
    float getMinX() const { return m_minX; }
    float getMinZ() const { return m_minZ; }
    
    const std::vector<bool>& getGrid() const { return m_walkableGrid; }
    
private:
    float m_minX, m_maxX, m_minZ, m_maxZ;
    float m_cellSize;
    int m_width, m_depth;
    std::vector<bool> m_walkableGrid; // Flat array of size m_width * m_depth
    
    int getIndex(int cx, int cz) const {
        return cz * m_width + cx;
    }
};

#endif
