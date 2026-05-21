#include "NavigationGrid.h"
#include "Entity.h"
#include <queue>
#include <cmath>
#include <algorithm>
#include <cstdlib>

NavigationGrid::NavigationGrid()
    : m_minX(-15.0f), m_maxX(15.0f), m_minZ(-15.0f), m_maxZ(15.0f),
      m_cellSize(1.0f), m_width(30), m_depth(30) {
    m_walkableGrid.assign(m_width * m_depth, true);
}

void NavigationGrid::initialize(float minX, float maxX, float minZ, float maxZ, float cellSize) {
    m_minX = minX;
    m_maxX = maxX;
    m_minZ = minZ;
    m_maxZ = maxZ;
    m_cellSize = cellSize;
    
    m_width = (int)std::ceil((m_maxX - m_minX) / m_cellSize);
    m_depth = (int)std::ceil((m_maxZ - m_minZ) / m_cellSize);
    
    m_walkableGrid.assign(m_width * m_depth, true);
}

void NavigationGrid::buildObstacles(const std::vector<Entity>& entities) {
    // Start with all cells walkable
    m_walkableGrid.assign(m_width * m_depth, true);
    
    // Check cell overlap with static block entities
    for (int cz = 0; cz < m_depth; ++cz) {
        for (int cx = 0; cx < m_width; ++cx) {
            glm::vec3 cellCenter = cellToWorld(cx, cz);
            float hs = m_cellSize * 0.5f;
            
            // Define cell AABB bounding box (Y bounds are high to catch standard walls)
            AABB cellAABB(
                glm::vec3(cellCenter.x - hs, -10.0f, cellCenter.z - hs),
                glm::vec3(cellCenter.x + hs,  10.0f, cellCenter.z + hs)
            );
            
            bool blocked = false;
            for (const auto& e : entities) {
                // We check entities that acts as rigid blocks
                if (!e.visible || !e.hasCollision || e.type == WATER || e.type == FLOOR || e.isLight || e.isAI) {
                    continue;
                }
                
                // Only consider walls, obstacle cubes, furniture or model boundaries
                std::string lowerName = e.name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                
                bool isBlocker = (lowerName.find("wall") != std::string::npos) ||
                                 (lowerName.find("obstacle") != std::string::npos) ||
                                 (lowerName.find("box") != std::string::npos) ||
                                 (lowerName.find("fence") != std::string::npos) ||
                                 (lowerName.find("post") != std::string::npos) ||
                                 (lowerName.find("pillar") != std::string::npos) ||
                                 (lowerName.find("platform") != std::string::npos);
                                 
                if (isBlocker) {
                    AABB globalBounds = e.getGlobalBounds();
                    
                    // If the obstacle is low enough to jump over (<= 0.8m), do not block A* grid
                    if (globalBounds.maxExtents.y <= 0.8f) {
                        continue;
                    }
                    
                    // If the obstacle is hovering high with bottom clearance > 1.2m, do not block A* grid (can walk under)
                    if (globalBounds.minExtents.y > 1.2f) {
                        continue;
                    }
                    
                    // Expand bounds slightly to prevent agents from clipping corners
                    glm::vec3 padding(0.45f, 0.0f, 0.45f);
                    AABB paddedBounds(globalBounds.minExtents - padding, globalBounds.maxExtents + padding);
                    
                    if (cellAABB.intersects(paddedBounds)) {
                        blocked = true;
                        break;
                    }
                }
            }
            
            if (blocked) {
                m_walkableGrid[getIndex(cx, cz)] = false;
            }
        }
    }
}

bool NavigationGrid::isWalkableCell(int cx, int cz) const {
    if (cx < 0 || cx >= m_width || cz < 0 || cz >= m_depth) {
        return false;
    }
    return m_walkableGrid[getIndex(cx, cz)];
}

bool NavigationGrid::isWalkable(float x, float z) const {
    glm::ivec2 cell = worldToCell(glm::vec3(x, 0.0f, z));
    return isWalkableCell(cell.x, cell.y);
}

glm::ivec2 NavigationGrid::worldToCell(glm::vec3 worldPos) const {
    int cx = (int)std::floor((worldPos.x - m_minX) / m_cellSize);
    int cz = (int)std::floor((worldPos.z - m_minZ) / m_cellSize);
    cx = std::max(0, std::min(cx, m_width - 1));
    cz = std::max(0, std::min(cz, m_depth - 1));
    return glm::ivec2(cx, cz);
}

glm::vec3 NavigationGrid::cellToWorld(int cx, int cz) const {
    float x = m_minX + ((float)cx + 0.5f) * m_cellSize;
    float z = m_minZ + ((float)cz + 0.5f) * m_cellSize;
    return glm::vec3(x, 0.0f, z);
}

std::vector<glm::vec3> NavigationGrid::findPath(glm::vec3 start, glm::vec3 end) const {
    glm::ivec2 startCell = worldToCell(start);
    glm::ivec2 endCell = worldToCell(end);
    
    if (startCell == endCell) {
        return { end };
    }
    
    // Priority queue of nodes to expand
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
    
    std::vector<float> gScore(m_width * m_depth, 1e9f);
    std::vector<bool> closedSet(m_width * m_depth, false);
    
    // Flat parent map store
    std::vector<glm::ivec2> parentMap(m_width * m_depth, glm::ivec2(-1, -1));
    
    int startIdx = getIndex(startCell.x, startCell.y);
    gScore[startIdx] = 0.0f;
    
    float hStart = (float)(std::abs(endCell.x - startCell.x) + std::abs(endCell.y - startCell.y));
    openSet.push(AStarNode(startCell.x, startCell.y, 0.0f, hStart));
    
    bool found = false;
    while (!openSet.empty()) {
        AStarNode current = openSet.top();
        openSet.pop();
        
        int currIdx = getIndex(current.x, current.z);
        if (closedSet[currIdx]) continue;
        closedSet[currIdx] = true;
        
        if (current.x == endCell.x && current.z == endCell.y) {
            found = true;
            break;
        }
        
        // 4-way neighbors
        const int dx[] = { 1, -1, 0, 0 };
        const int dz[] = { 0, 0, 1, -1 };
        
        for (int i = 0; i < 4; ++i) {
            int nx = current.x + dx[i];
            int nz = current.z + dz[i];
            
            if (nx < 0 || nx >= m_width || nz < 0 || nz >= m_depth) continue;
            int nIdx = getIndex(nx, nz);
            if (closedSet[nIdx]) continue;
            if (!m_walkableGrid[nIdx]) continue; // Obstacle!
            
            float tentativeG = gScore[currIdx] + 1.0f;
            if (tentativeG < gScore[nIdx]) {
                gScore[nIdx] = tentativeG;
                parentMap[nIdx] = glm::ivec2(current.x, current.z);
                
                float h = (float)(std::abs(endCell.x - nx) + std::abs(endCell.y - nz));
                openSet.push(AStarNode(nx, nz, tentativeG, h, current.x, current.z));
            }
        }
    }
    
    std::vector<glm::vec3> path;
    if (found) {
        glm::ivec2 curr = endCell;
        while (curr.x != -1 && curr.y != -1) {
            path.push_back(cellToWorld(curr.x, curr.y));
            int idx = getIndex(curr.x, curr.y);
            curr = parentMap[idx];
        }
        std::reverse(path.begin(), path.end());
        
        // Adjust the very start and very end position of the path to exactly match start/end
        if (!path.empty()) {
            path[0] = start;
            path.push_back(end);
        }
    }
    
    return path;
}

glm::vec3 NavigationGrid::getRandomWalkablePosition() const {
    std::vector<glm::ivec2> walkableCells;
    walkableCells.reserve(m_width * m_depth);
    
    for (int cz = 0; cz < m_depth; ++cz) {
        for (int cx = 0; cx < m_width; ++cx) {
            if (m_walkableGrid[getIndex(cx, cz)]) {
                walkableCells.push_back(glm::ivec2(cx, cz));
            }
        }
    }
    
    if (walkableCells.empty()) {
        return glm::vec3(0.0f);
    }
    
    int randIdx = std::rand() % walkableCells.size();
    glm::ivec2 cell = walkableCells[randIdx];
    return cellToWorld(cell.x, cell.y);
}
