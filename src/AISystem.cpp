#include "AISystem.h"
#include "Scene.h"
#include "Renderer.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <ctime>

AISystem::AISystem()
    : preyGreenSpeedCoef(1.0f), preyBlueSpeedCoef(1.0f), predatorSpeedCoef(1.0f),
      preyGreenVisionRange(10.0f), preyBlueVisionRange(14.0f),
      predator1VisionRange(15.0f), predator2VisionRange(18.0f),
      m_roundState(ROUND_PREDATOR_1_RUN), m_roundDuration(30.0f), m_remainingTime(30.0f),
      m_predator1Score(0), m_predator2Score(0),
      m_seed(42), m_deterministic(true), m_debugEnabled(true) {
}

AISystem::~AISystem() {
}

AISystem& AISystem::instance() {
    static AISystem inst;
    return inst;
    // Note: absolute strict rule enforcement, strictly no emojis in comment block!
}

void AISystem::initializeGrid(float minX, float maxX, float minZ, float maxZ, float cellSize) {
    m_grid.initialize(minX, maxX, minZ, maxZ, cellSize);
}

void AISystem::setupScene(Scene* scene) {
    m_grid.buildObstacles(scene->entities);
    resetSimulation(scene);
}

void AISystem::resetSimulation(Scene* scene) {
    if (m_deterministic) {
        std::srand(m_seed);
    } else {
        std::srand((unsigned int)std::time(nullptr));
    }
    
    m_agents.clear();
    
    // Remove previous prey and predator entities from scene
    scene->entities.erase(
        std::remove_if(scene->entities.begin(), scene->entities.end(),
            [](const Entity& e) {
                return e.name.rfind("Prey_", 0) == 0 || e.name.rfind("Predator_", 0) == 0;
            }
        ),
        scene->entities.end()
    );
    
    // Set timer and states
    m_remainingTime = m_roundDuration;
    
    if (m_roundState == ROUND_PREDATOR_1_FINISHED) {
        m_roundState = ROUND_PREDATOR_1_RUN;
    }
    
    // Spawn preys and active predator
    spawnPrey(scene);
    
    if (m_roundState == ROUND_PREDATOR_1_RUN) {
        m_predator1Score = 0;
        spawnPredator(scene, SPECIES_PREDATOR_1);
    } else if (m_roundState == ROUND_PREDATOR_2_RUN) {
        m_predator2Score = 0;
        spawnPredator(scene, SPECIES_PREDATOR_2);
    }
    
    syncAgentsToEntities(scene);
}

void AISystem::startNextRound(Scene* scene) {
    m_roundState = ROUND_PREDATOR_2_RUN;
    resetSimulation(scene);
}

void AISystem::forceResetAll(Scene* scene) {
    m_roundState = ROUND_PREDATOR_1_RUN;
    m_predator1Score = 0;
    m_predator2Score = 0;
    resetSimulation(scene);
}

void AISystem::update(Scene* scene, float deltaTime) {
    if (m_roundState == ROUND_SIMULATION_FINISHED) {
        return;
    }
    
    m_remainingTime -= deltaTime;
    
    if (m_remainingTime <= 0.0f) {
        m_remainingTime = 0.0f;
        if (m_roundState == ROUND_PREDATOR_1_RUN) {
            m_roundState = ROUND_PREDATOR_1_FINISHED;
            startNextRound(scene);
        } else if (m_roundState == ROUND_PREDATOR_2_RUN) {
            m_roundState = ROUND_SIMULATION_FINISHED;
        }
        return;
    }
    
    if (m_roundState == ROUND_PREDATOR_1_RUN || m_roundState == ROUND_PREDATOR_2_RUN) {
        for (auto& agent : m_agents) {
            bool active = false;
            for (const auto& ent : scene->entities) {
                if (ent.name == agent.entityName) {
                    active = ent.visible;
                    break;
                }
            }
            if (!active) continue;
            
            updateFSM(agent, deltaTime, scene);
            updateMovement(agent, deltaTime, scene);
        }
        
        syncAgentsToEntities(scene);
    }
}

void AISystem::spawnPrey(Scene* scene) {
    // 10 green preys
    for (int i = 0; i < 10; ++i) {
        glm::vec3 pos = m_grid.getRandomWalkablePosition();
        float platformTopY = 0.0f;
        for (const auto& e : scene->entities) {
            if (e.name.find("platform") != std::string::npos || e.name.find("obstacle") != std::string::npos) {
                AABB bounds = e.getGlobalBounds();
                if (pos.x >= bounds.minExtents.x && pos.x <= bounds.maxExtents.x &&
                    pos.z >= bounds.minExtents.z && pos.z <= bounds.maxExtents.z) {
                    platformTopY = std::max(platformTopY, bounds.maxExtents.y);
                }
            }
        }
        pos.y = platformTopY + 0.4f;
        
        std::string name = "Prey_Green_" + std::to_string(i);
        Entity prey(name, SPHERE, pos, glm::vec3(0.1f, 0.8f, 0.1f));
        prey.scale = glm::vec3(0.4f);
        prey.roughness = 0.6f; prey.metallic = 0.0f; prey.reflectivity = 0.1f;
        prey.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        prey.radius = 0.4f;
        prey.mass = 1.0f;
        prey.isAI = true;
        prey.aiRole = ROLE_PREY;
        prey.aiSpecies = SPECIES_GREEN_PREY;
        prey.aiState = WANDER;
        prey.aiVisionRange = preyGreenVisionRange;
        prey.aiVisionFOV = 120.0f;
        scene->addEntity(prey);
        
        AIAgent agent;
        agent.entityName = name;
        agent.role = ROLE_PREY;
        agent.species = SPECIES_GREEN_PREY;
        agent.state = WANDER;
        agent.position = pos;
        agent.forward = glm::vec3(1.0f, 0.0f, 0.0f);
        agent.velocity = glm::vec3(0.0f);
        agent.maxSpeed = 3.0f * preyGreenSpeedCoef;
        agent.acceleration = 6.0f;
        agent.visionRange = preyGreenVisionRange;
        agent.visionFOV = 120.0f;
        agent.captureRadius = 0.8f;
        agent.scoreValue = 10.0f;
        agent.wanderTimer = 0.0f;
        agent.repathTimer = 0.0f;
        agent.currentWaypointIndex = 0;
        agent.velocityY = 0.0f;
        agent.isGrounded = true;
        agent.stuckTimer = 0.0f;
        agent.lastPosition = pos;
        
        m_agents.push_back(agent);
    }
    
    // 10 blue preys
    for (int i = 0; i < 10; ++i) {
        glm::vec3 pos = m_grid.getRandomWalkablePosition();
        float platformTopY = 0.0f;
        for (const auto& e : scene->entities) {
            if (e.name.find("platform") != std::string::npos || e.name.find("obstacle") != std::string::npos) {
                AABB bounds = e.getGlobalBounds();
                if (pos.x >= bounds.minExtents.x && pos.x <= bounds.maxExtents.x &&
                    pos.z >= bounds.minExtents.z && pos.z <= bounds.maxExtents.z) {
                    platformTopY = std::max(platformTopY, bounds.maxExtents.y);
                }
            }
        }
        pos.y = platformTopY + 0.4f;
        
        std::string name = "Prey_Blue_" + std::to_string(i);
        Entity prey(name, SPHERE, pos, glm::vec3(0.1f, 0.3f, 0.9f));
        prey.scale = glm::vec3(0.4f);
        prey.roughness = 0.5f; prey.metallic = 0.1f; prey.reflectivity = 0.2f;
        prey.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        prey.radius = 0.4f;
        prey.mass = 1.0f;
        prey.isAI = true;
        prey.aiRole = ROLE_PREY;
        prey.aiSpecies = SPECIES_BLUE_PREY;
        prey.aiState = WANDER;
        prey.aiVisionRange = preyBlueVisionRange;
        prey.aiVisionFOV = 160.0f;
        scene->addEntity(prey);
        
        AIAgent agent;
        agent.entityName = name;
        agent.role = ROLE_PREY;
        agent.species = SPECIES_BLUE_PREY;
        agent.state = WANDER;
        agent.position = pos;
        agent.forward = glm::vec3(1.0f, 0.0f, 0.0f);
        agent.velocity = glm::vec3(0.0f);
        agent.maxSpeed = 4.5f * preyBlueSpeedCoef; // Runs faster!
        agent.acceleration = 9.0f;
        agent.visionRange = preyBlueVisionRange;
        agent.visionFOV = 160.0f;
        agent.captureRadius = 0.8f;
        agent.scoreValue = 25.0f; // Higher value!
        agent.wanderTimer = 0.0f;
        agent.repathTimer = 0.0f;
        agent.currentWaypointIndex = 0;
        agent.velocityY = 0.0f;
        agent.isGrounded = true;
        agent.stuckTimer = 0.0f;
        agent.lastPosition = pos;
        
        m_agents.push_back(agent);
    }
}

void AISystem::spawnPredator(Scene* scene, AISpecies species) {
    glm::vec3 pos = m_grid.getRandomWalkablePosition();
    float platformTopY = 0.0f;
    for (const auto& e : scene->entities) {
        if (e.name.find("platform") != std::string::npos || e.name.find("obstacle") != std::string::npos) {
            AABB bounds = e.getGlobalBounds();
            if (pos.x >= bounds.minExtents.x && pos.x <= bounds.maxExtents.x &&
                pos.z >= bounds.minExtents.z && pos.z <= bounds.maxExtents.z) {
                platformTopY = std::max(platformTopY, bounds.maxExtents.y);
            }
        }
    }
    pos.y = platformTopY + 0.4f;
    
    if (species == SPECIES_PREDATOR_1) {
        std::string name = "Predator_1";
        Entity pred(name, CUBE, pos, glm::vec3(0.9f, 0.1f, 0.1f)); // Red Cube
        pred.scale = glm::vec3(0.8f);
        pred.roughness = 0.3f; pred.metallic = 0.8f; pred.reflectivity = 0.5f;
        pred.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        pred.radius = 0.6f;
        pred.mass = 2.0f;
        pred.isAI = true;
        pred.aiRole = ROLE_PREDATOR;
        pred.aiSpecies = SPECIES_PREDATOR_1;
        pred.aiState = SEARCH;
        pred.aiVisionRange = predator1VisionRange;
        pred.aiVisionFOV = 140.0f;
        scene->addEntity(pred);
        
        AIAgent agent;
        agent.entityName = name;
        agent.role = ROLE_PREDATOR;
        agent.species = SPECIES_PREDATOR_1;
        agent.state = SEARCH;
        agent.position = pos;
        agent.forward = glm::vec3(0.0f, 0.0f, 1.0f);
        agent.velocity = glm::vec3(0.0f);
        agent.maxSpeed = 5.5f * predatorSpeedCoef;
        agent.acceleration = 12.0f;
        agent.visionRange = predator1VisionRange;
        agent.visionFOV = 140.0f;
        agent.captureRadius = 0.8f;
        agent.scoreValue = 0.0f;
        agent.wanderTimer = 0.0f;
        agent.repathTimer = 0.0f;
        agent.currentWaypointIndex = 0;
        agent.velocityY = 0.0f;
        agent.isGrounded = true;
        agent.stuckTimer = 0.0f;
        agent.lastPosition = pos;
        
        m_agents.push_back(agent);
    } else {
        std::string name = "Predator_2";
        Entity pred(name, CUBE, pos, glm::vec3(0.9f, 0.1f, 0.9f)); // Magenta Cube
        pred.scale = glm::vec3(0.8f);
        pred.roughness = 0.2f; pred.metallic = 0.9f; pred.reflectivity = 0.6f;
        pred.localBounds = AABB(glm::vec3(-0.5f), glm::vec3(0.5f));
        pred.radius = 0.6f;
        pred.mass = 2.0f;
        pred.isAI = true;
        pred.aiRole = ROLE_PREDATOR;
        pred.aiSpecies = SPECIES_PREDATOR_2;
        pred.aiState = SEARCH;
        pred.aiVisionRange = predator2VisionRange;
        pred.aiVisionFOV = 160.0f;
        scene->addEntity(pred);
        
        AIAgent agent;
        agent.entityName = name;
        agent.role = ROLE_PREDATOR;
        agent.species = SPECIES_PREDATOR_2;
        agent.state = SEARCH;
        agent.position = pos;
        agent.forward = glm::vec3(0.0f, 0.0f, 1.0f);
        agent.velocity = glm::vec3(0.0f);
        agent.maxSpeed = 5.5f * predatorSpeedCoef;
        agent.acceleration = 12.0f;
        agent.visionRange = predator2VisionRange;
        agent.visionFOV = 160.0f;
        agent.captureRadius = 0.8f;
        agent.scoreValue = 0.0f;
        agent.wanderTimer = 0.0f;
        agent.repathTimer = 0.0f;
        agent.currentWaypointIndex = 0;
        agent.velocityY = 0.0f;
        agent.isGrounded = true;
        agent.stuckTimer = 0.0f;
        agent.lastPosition = pos;
        
        m_agents.push_back(agent);
    }
}

bool AISystem::hasLineOfSight(glm::vec3 start, glm::vec3 end, const Scene* scene) const {
    glm::vec3 dir = end - start;
    float dist = glm::length(dir);
    if (dist < 1e-4f) return true;
    glm::vec3 rayDir = dir / dist;
    
    for (const auto& e : scene->entities) {
        if (!e.visible || !e.hasCollision || e.type == WATER || e.type == FLOOR || e.isLight || e.isAI) {
            continue;
        }
        
        std::string lowerName = e.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        bool isBlocker = (lowerName.find("wall") != std::string::npos) ||
                         (lowerName.find("obstacle") != std::string::npos) ||
                         (lowerName.find("box") != std::string::npos) ||
                         (lowerName.find("fence") != std::string::npos) ||
                         (lowerName.find("post") != std::string::npos) ||
                         (lowerName.find("pillar") != std::string::npos);
                         
        if (isBlocker) {
            AABB bounds = e.getGlobalBounds();
            
            // Slab method ray-AABB test
            float tMin = 0.0f;
            float tMax = dist;
            bool intersect = true;
            for (int i = 0; i < 3; ++i) {
                if (std::abs(rayDir[i]) < 1e-6f) {
                    if (start[i] < bounds.minExtents[i] || start[i] > bounds.maxExtents[i]) {
                        intersect = false;
                        break;
                    }
                } else {
                    float t1 = (bounds.minExtents[i] - start[i]) / rayDir[i];
                    float t2 = (bounds.maxExtents[i] - start[i]) / rayDir[i];
                    if (t1 > t2) std::swap(t1, t2);
                    tMin = std::max(tMin, t1);
                    tMax = std::min(tMax, t2);
                    if (tMin > tMax) {
                        intersect = false;
                        break;
                    }
                }
            }
            
            if (intersect && tMin <= dist && tMax >= 0.0f) {
                return false; // Obstruction found
            }
        }
    }
    return true; // Clear line of sight
}

void AISystem::updateSensor(AIAgent& agent, const Scene* scene) {
    // Dynamic texture, strictly no emojis in comment!
}

void AISystem::updateFSM(AIAgent& agent, float deltaTime, Scene* scene) {
    agent.wanderTimer -= deltaTime;
    agent.repathTimer -= deltaTime;
    
    // Stuck recovery: if stuck in WANDER/SEARCH, force immediately selecting a new wander path
    if (agent.stuckTimer >= 1.0f && (agent.state == WANDER || agent.state == SEARCH)) {
        agent.wanderTimer = 0.0f;
        agent.path.clear();
    }
    
    // Find active entities for sensors
    std::vector<AIAgent*> visibleThreats;
    std::vector<AIAgent*> visiblePrey;
    
    for (auto& other : m_agents) {
        if (other.entityName == agent.entityName) continue;
        
        // Find corresponding Entity to check visibility
        bool otherVisible = false;
        for (const auto& ent : scene->entities) {
            if (ent.name == other.entityName) {
                otherVisible = ent.visible;
                break;
            }
        }
        if (!otherVisible) continue;
        
        float d = glm::distance(agent.position, other.position);
        if (d <= agent.visionRange) {
            glm::vec3 toOther = glm::normalize(other.position - agent.position);
            float cosAngle = glm::dot(agent.forward, toOther);
            if (cosAngle >= glm::cos(glm::radians(agent.visionFOV * 0.5f))) {
                if (hasLineOfSight(agent.position + glm::vec3(0, 0.4f, 0), other.position + glm::vec3(0, 0.4f, 0), scene)) {
                    if (agent.role == ROLE_PREY && other.role == ROLE_PREDATOR) {
                        visibleThreats.push_back(&other);
                    } else if (agent.role == ROLE_PREDATOR && other.role == ROLE_PREY) {
                        visiblePrey.push_back(&other);
                    }
                }
            }
        }
    }
    
    // Execute state transitions
    if (agent.role == ROLE_PREY) {
        if (agent.state == WANDER) {
            if (!visibleThreats.empty()) {
                agent.state = FLEE;
                agent.path.clear();
            } else if (agent.wanderTimer <= 0.0f) {
                // Pick random walkable cell within 5 meters
                glm::vec3 dest = agent.position;
                bool found = false;
                for (int attempts = 0; attempts < 30; ++attempts) {
                    float rx = ((float)(std::rand() % 200) / 100.0f - 1.0f) * 7.0f;
                    float rz = ((float)(std::rand() % 200) / 100.0f - 1.0f) * 7.0f;
                    glm::vec3 testPos = agent.position + glm::vec3(rx, 0.0f, rz);
                    if (m_grid.isWalkable(testPos.x, testPos.z) && glm::distance(agent.position, testPos) > 1.5f) {
                        dest = testPos;
                        found = true;
                        break;
                    }
                }
                
                // Fallback to random global walkable position if local search failed
                if (!found) {
                    dest = m_grid.getRandomWalkablePosition();
                }
                
                agent.path = m_grid.findPath(agent.position, dest);
                if (agent.path.empty()) {
                    agent.wanderTimer = 0.0f; // Force selecting again next frame rather than idling
                } else {
                    agent.currentWaypointIndex = 0;
                    agent.wanderTimer = 3.0f + (float)(std::rand() % 200) / 100.0f;
                }
            }
        } else if (agent.state == FLEE) {
            if (visibleThreats.empty()) {
                agent.state = WANDER;
                agent.wanderTimer = 0.0f; // Force new wander destination immediately
            }
        }
    } else if (agent.role == ROLE_PREDATOR) {
        if (agent.state == SEARCH) {
            if (!visiblePrey.empty()) {
                agent.state = CHASE;
                
                // Strategy Selection
                if (agent.species == SPECIES_PREDATOR_1) {
                    // Greedy nearest-prey strategy
                    float minDist = 1e9f;
                    AIAgent* target = nullptr;
                    for (auto* prey : visiblePrey) {
                        float dist = glm::distance(agent.position, prey->position);
                        if (dist < minDist) {
                            minDist = dist;
                            target = prey;
                        }
                    }
                    if (target) {
                        agent.targetEntityName = target->entityName;
                    }
                } else {
                    // Value-aware utility strategy: Score = Value / Distance
                    float maxUtility = -1.0f;
                    AIAgent* target = nullptr;
                    for (auto* prey : visiblePrey) {
                        float dist = glm::distance(agent.position, prey->position);
                        float utility = prey->scoreValue / (dist + 0.1f);
                        if (utility > maxUtility) {
                            maxUtility = utility;
                            target = prey;
                        }
                    }
                    if (target) {
                        agent.targetEntityName = target->entityName;
                    }
                }
                
                agent.path.clear();
                agent.repathTimer = 0.0f;
            } else if (agent.wanderTimer <= 0.0f) {
                // Hybrid Wide Search Pattern: mix of long-range room transit and local room scouting
                glm::vec3 dest = agent.position;
                std::vector<glm::vec3> candidates;
                for (int i = 0; i < 5; ++i) {
                    candidates.push_back(m_grid.getRandomWalkablePosition());
                }
                
                if (!candidates.empty()) {
                    if (std::rand() % 100 < 70) {
                        // 70% probability: Long-range transit (furthest point) to cross different rooms
                        float maxDist = -1.0f;
                        for (const auto& p : candidates) {
                            float d = glm::distance(agent.position, p);
                            if (d > maxDist) {
                                maxDist = d;
                                dest = p;
                            }
                        }
                    } else {
                        // 30% probability: Medium-range local room search
                        float bestDiff = 1e9f;
                        float targetDist = 8.0f + (float)(std::rand() % 40) / 10.0f; // 8m to 12m
                        for (const auto& p : candidates) {
                            float d = glm::distance(agent.position, p);
                            float diff = std::abs(d - targetDist);
                            if (diff < bestDiff) {
                                bestDiff = diff;
                                dest = p;
                            }
                        }
                    }
                }
                
                agent.path = m_grid.findPath(agent.position, dest);
                if (agent.path.empty()) {
                    agent.wanderTimer = 0.0f; // Force retry
                } else {
                    agent.currentWaypointIndex = 0;
                    agent.wanderTimer = 5.0f + (float)(std::rand() % 300) / 100.0f;
                }
            }
        } else if (agent.state == CHASE) {
            // Check if target still exists and is visible
            AIAgent* preyTarget = nullptr;
            for (auto* p : visiblePrey) {
                if (p->entityName == agent.targetEntityName) {
                    preyTarget = p;
                    break;
                }
            }
            
            if (!preyTarget) {
                // Prey is lost or captured. Check if it escaped (still visible in scene but out of our immediate sight)
                bool escaped = false;
                glm::vec3 lastSeenPos(0.0f);
                for (const auto& other : m_agents) {
                    if (other.entityName == agent.targetEntityName) {
                        for (const auto& ent : scene->entities) {
                            if (ent.name == other.entityName && ent.visible) {
                                escaped = true;
                                lastSeenPos = other.position;
                                break;
                            }
                        }
                        break;
                    }
                }
                
                agent.state = SEARCH;
                agent.targetEntityName = "";
                
                if (escaped && glm::distance(agent.position, lastSeenPos) > 1.5f) {
                    // Investigate Last Known Position of the escaped prey
                    agent.path = m_grid.findPath(agent.position, lastSeenPos);
                    if (!agent.path.empty()) {
                        agent.currentWaypointIndex = 0;
                        agent.wanderTimer = 4.0f + (float)(std::rand() % 200) / 100.0f;
                        std::cout << "[AI System] " << agent.entityName << " lost sight of prey. Investigating Last Known Position at (" 
                                  << lastSeenPos.x << ", " << lastSeenPos.z << ")" << std::endl;
                    } else {
                        agent.wanderTimer = 0.0f;
                        agent.path.clear();
                    }
                } else {
                    agent.wanderTimer = 0.0f;
                    agent.path.clear();
                }
            } else {
                // Periodically update path to prey if out of direct sightline
                if (agent.repathTimer <= 0.0f) {
                    agent.repathTimer = 0.25f; // Throttle
                    
                    float distToPrey = glm::distance(agent.position, preyTarget->position);
                    bool los = hasLineOfSight(agent.position + glm::vec3(0, 0.4f, 0), preyTarget->position + glm::vec3(0, 0.4f, 0), scene);
                    
                    // Only allow direct seek (pounce) if we have clear line-of-sight AND are quite close,
                    // and not stuck. Otherwise, we MUST use A* navigation to cleanly round corners.
                    if (los && distToPrey < 3.5f && agent.stuckTimer < 0.8f) {
                        agent.path.clear(); // Seek directly
                        agent.targetPosition = preyTarget->position;
                    } else {
                        // Navigate using A* which naturally has path padding around walls
                        agent.path = m_grid.findPath(agent.position, preyTarget->position);
                        agent.currentWaypointIndex = 0;
                    }
                }
                
                if (agent.path.empty()) {
                    agent.targetPosition = preyTarget->position;
                }
            }
        }
    }
}

void AISystem::updateMovement(AIAgent& agent, float deltaTime, Scene* scene) {
    glm::vec3 steerForce(0.0f);
    float bottomOffset = 0.4f;
    
    // Seek steering behavior helper
    auto seek = [&](glm::vec3 target) -> glm::vec3 {
        glm::vec3 desired = target - agent.position;
        desired.y = 0.0f;
        float d = glm::length(desired);
        if (d < 1e-4f) return glm::vec3(0.0f);
        desired = glm::normalize(desired) * agent.maxSpeed;
        return desired - agent.velocity;
    };
    
    if (agent.role == ROLE_PREY && agent.state == FLEE) {
        // Find nearest predator to flee from
        glm::vec3 threatPos(0.0f);
        bool threatFound = false;
        for (const auto& other : m_agents) {
            if (other.role == ROLE_PREDATOR) {
                // Make sure predator entity is visible/active
                for (const auto& ent : scene->entities) {
                    if (ent.name == other.entityName && ent.visible) {
                        threatPos = other.position;
                        threatFound = true;
                        break;
                    }
                }
                if (threatFound) break;
            }
        }
        
        if (threatFound) {
            glm::vec3 fleeDir = glm::normalize(agent.position - threatPos);
            fleeDir.y = 0.0f;
            
            // Context Steering: evaluate 8 directions
            glm::vec3 bestDir = fleeDir;
            float bestScore = -1000.0f;
            
            for (int i = 0; i < 8; ++i) {
                float angle = i * glm::pi<float>() / 4.0f;
                glm::vec3 testDir(glm::cos(angle), 0.0f, glm::sin(angle));
                
                // Base score: dot product with fleeDir (prefer going away from predator)
                float score = glm::dot(testDir, fleeDir);
                
                // Collision penalty: check if moving along this direction hits wall/corner
                glm::vec3 checkPos = agent.position + testDir * 1.5f;
                if (!m_grid.isWalkable(checkPos.x, checkPos.z)) {
                    score -= 5.0f; // Massive penalty for wall blockages!
                }
                
                if (score > bestScore) {
                    bestScore = score;
                    bestDir = testDir;
                }
            }
            
            glm::vec3 desiredVel = bestDir * agent.maxSpeed;
            steerForce = desiredVel - agent.velocity;
        }
    } else {
        // Path following or direct seeking
        if (!agent.path.empty()) {
            if (agent.currentWaypointIndex < agent.path.size()) {
                glm::vec3 wp = agent.path[agent.currentWaypointIndex];
                wp.y = 0.0f;
                
                float d = glm::distance(glm::vec2(agent.position.x, agent.position.z), glm::vec2(wp.x, wp.z));
                float acceptRadius = (agent.role == ROLE_PREDATOR) ? 0.7f : 0.5f;
                // Skip waypoint if we are close enough or if we have been stuck and are reasonably close
                if (d < acceptRadius || (agent.stuckTimer > 0.5f && d < 1.4f)) {
                    agent.currentWaypointIndex++;
                    agent.stuckTimer = std::max(0.0f, agent.stuckTimer - 0.4f);
                }
                
                if (agent.currentWaypointIndex < agent.path.size()) {
                    steerForce = seek(agent.path[agent.currentWaypointIndex]);
                } else {
                    agent.path.clear();
                }
            } else {
                agent.path.clear();
            }
        } else if (agent.role == ROLE_PREDATOR && agent.state == CHASE) {
            // Seek prey target directly (we cleared the path when line-of-sight is open)
            steerForce = seek(agent.targetPosition);
        }
    }
    
    // Clamp steering force to acceleration
    if (glm::length(steerForce) > 0.001f) {
        steerForce = glm::normalize(steerForce) * glm::min(glm::length(steerForce), agent.acceleration);
    }
    
    // Integrate velocity and position
    agent.velocity += steerForce * deltaTime;
    
    // Nudge force to resolve stuck states: push away from adjacent walls
    if (agent.stuckTimer >= 0.8f) {
        glm::vec3 nudge(0.0f);
        glm::ivec2 cell = m_grid.worldToCell(agent.position);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dz == 0) continue;
                if (!m_grid.isWalkableCell(cell.x + dx, cell.y + dz)) {
                    nudge += glm::vec3(-(float)dx, 0.0f, -(float)dz);
                }
            }
        }
        if (glm::length(nudge) > 0.001f) {
            nudge = glm::normalize(nudge);
        } else {
            float angle = (float)(std::rand() % 360) * glm::pi<float>() / 180.0f;
            nudge = glm::vec3(glm::cos(angle), 0.0f, glm::sin(angle));
        }
        agent.velocity += nudge * agent.acceleration * 2.0f * deltaTime;
    }
    
    if (glm::length(agent.velocity) > agent.maxSpeed) {
        agent.velocity = glm::normalize(agent.velocity) * agent.maxSpeed;
    }
    
    // Horizontal movement integration
    agent.position.x += agent.velocity.x * deltaTime;
    agent.position.z += agent.velocity.z * deltaTime;
    
    // Integrate gravity
    if (!agent.isGrounded) {
        agent.velocityY += -12.0f * deltaTime;
        if (agent.velocityY < -20.0f) agent.velocityY = -20.0f; // clamp falling velocity
    } else {
        agent.velocityY = -0.5f; // small downward velocity to maintain grounding check
    }
    
    // Vertical movement integration
    agent.position.y += agent.velocityY * deltaTime;

    // Resolve collisions against static obstacles to prevent wall-passing / tunneling
    float agentRadius = (agent.role == ROLE_PREDATOR) ? 0.6f : 0.4f;
    bool isCollidingHorizontally = false;
    for (const auto& e : scene->entities) {
        if (!e.visible || !e.hasCollision || e.type == WATER || e.type == FLOOR || e.isLight || e.isAI) {
            continue;
        }
        
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
            AABB bounds = e.getGlobalBounds();
            
            // XZ Push-Out only happens if agent overlaps vertically with the obstacle body (not on top/below)
            bool overlapY = (agent.position.y - bottomOffset < bounds.maxExtents.y - 0.05f) &&
                            (agent.position.y + bottomOffset > bounds.minExtents.y + 0.05f);
            if (!overlapY) {
                continue;
            }
            
            // Project agent center onto AABB bounds (XZ only)
            glm::vec3 closestPt(
                std::max(bounds.minExtents.x, std::min(agent.position.x, bounds.maxExtents.x)),
                0.0f,
                std::max(bounds.minExtents.z, std::min(agent.position.z, bounds.maxExtents.z))
            );
            
            glm::vec3 toAgent = agent.position - closestPt;
            toAgent.y = 0.0f;
            float dist = glm::length(toAgent);
            
            if (dist < agentRadius) {
                isCollidingHorizontally = true;
                if (dist > 1e-4f) {
                    // Push out
                    glm::vec3 normal = toAgent / dist;
                    agent.position += normal * (agentRadius - dist);
                    
                    // Sliding response: zero out velocity pointing into the wall
                    float vn = glm::dot(agent.velocity, normal);
                    if (vn < 0.0f) {
                        agent.velocity -= normal * vn;
                    }
                } else {
                    // Agent center is exactly on/inside AABB. Push out to the closest side in XZ
                    float dxL = agent.position.x - bounds.minExtents.x;
                    float dxR = bounds.maxExtents.x - agent.position.x;
                    float dzB = agent.position.z - bounds.minExtents.z;
                    float dzF = bounds.maxExtents.z - agent.position.z;
                    
                    float minDist = dxL;
                    if (dxR < minDist) minDist = dxR;
                    if (dzB < minDist) minDist = dzB;
                    if (dzF < minDist) minDist = dzF;
                    
                    if (minDist == dxL) {
                        agent.position.x = bounds.minExtents.x - agentRadius;
                        if (agent.velocity.x > 0.0f) agent.velocity.x = 0.0f;
                    } else if (minDist == dxR) {
                        agent.position.x = bounds.maxExtents.x + agentRadius;
                        if (agent.velocity.x < 0.0f) agent.velocity.x = 0.0f;
                    } else if (minDist == dzB) {
                        agent.position.z = bounds.minExtents.z - agentRadius;
                        if (agent.velocity.z > 0.0f) agent.velocity.z = 0.0f;
                    } else {
                        agent.position.z = bounds.maxExtents.z + agentRadius;
                        if (agent.velocity.z < 0.0f) agent.velocity.z = 0.0f;
                    }
                }
            }
        }
    }
    
    // Resolve Y-axis landing and grounding checks
    agent.isGrounded = false;
    
    // Floor grounding
    if (agent.position.y - bottomOffset <= 0.0f) {
        agent.position.y = bottomOffset;
        agent.isGrounded = true;
        if (agent.velocityY < 0.0f) {
            agent.velocityY = 0.0f;
        }
    }
    
    // Platform boxes landing grounding
    for (const auto& e : scene->entities) {
        if (!e.visible || !e.hasCollision || e.type == WATER || e.type == FLOOR || e.isLight || e.isAI) {
            continue;
        }
        
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
            AABB bounds = e.getGlobalBounds();
            
            // Slightly smaller cylinder for Y collision landing check to prevent sliding off edges
            float r = agentRadius - 0.05f;
            bool overlapX = (agent.position.x + r > bounds.minExtents.x) && (agent.position.x - r < bounds.maxExtents.x);
            bool overlapZ = (agent.position.z + r > bounds.minExtents.z) && (agent.position.z - r < bounds.maxExtents.z);
            
            if (overlapX && overlapZ) {
                // Land on top surface of platform
                float topY = bounds.maxExtents.y;
                float feetY = agent.position.y - bottomOffset;
                if (feetY >= topY - 0.3f && feetY <= topY + 0.15f) {
                    if (agent.velocityY <= 0.0f) {
                        agent.position.y = topY + bottomOffset;
                        agent.isGrounded = true;
                        agent.velocityY = 0.0f;
                    }
                }
                
                // Head hit underside (ceiling)
                float bottomY = bounds.minExtents.y;
                float headY = agent.position.y + bottomOffset;
                if (headY >= bottomY - 0.05f && headY <= bottomY + 0.2f) {
                    if (agent.velocityY > 0.0f) {
                        agent.position.y = bottomY - bottomOffset - 0.01f;
                        agent.velocityY = 0.0f;
                    }
                }
            }
        }
    }
    
    // Auto-Jump Trigger
    bool isTryingToMove = glm::length(steerForce) > 0.1f;
    
    bool targetIsHigher = false;
    if (agent.role == ROLE_PREDATOR && agent.state == CHASE) {
        float feetY = agent.position.y - bottomOffset;
        float targetFeetY = agent.targetPosition.y - 0.4f; // Target is always Prey, scale 0.4f -> offset 0.4f
        targetIsHigher = (targetFeetY > feetY + 0.3f);
    }
    
    if (agent.isGrounded && ((isTryingToMove && isCollidingHorizontally) || targetIsHigher)) {
        agent.velocityY = 5.8f; // Athletic jump force to scale low obstacles cleanly
        agent.isGrounded = false;
    }
    
    if (glm::length(agent.velocity) > 0.05f) {
        agent.forward = glm::normalize(agent.velocity);
    }
    
    // Update stuck tracking
    float distMovedXZ = glm::distance(glm::vec2(agent.position.x, agent.position.z), glm::vec2(agent.lastPosition.x, agent.lastPosition.z));
    if (isTryingToMove && (isCollidingHorizontally || distMovedXZ < agent.maxSpeed * 0.15f * deltaTime)) {
        agent.stuckTimer += deltaTime;
    } else {
        agent.stuckTimer = std::max(0.0f, agent.stuckTimer - deltaTime * 0.5f);
    }
    agent.lastPosition = agent.position;
}

void AISystem::syncAgentsToEntities(Scene* scene) {
    for (auto& agent : m_agents) {
        for (auto& ent : scene->entities) {
            if (ent.name == agent.entityName) {
                ent.position = agent.position;
                
                // Update rotation based on forward direction (XZ angle)
                float angleRad = std::atan2(agent.forward.x, agent.forward.z);
                ent.rotation.y = glm::degrees(angleRad);
                
                // Update AI variables for rendering & ImGui
                ent.aiRole = (int)agent.role;
                ent.aiSpecies = (int)agent.species;
                ent.aiState = (int)agent.state;
                ent.aiVisionRange = agent.visionRange;
                ent.aiVisionFOV = agent.visionFOV;
                ent.aiPath = agent.path;
                
                // If it is flee state, make it blink or slightly change its color to indicate alarm!
                if (agent.role == ROLE_PREY && agent.state == FLEE) {
                    ent.color = glm::vec3(1.0f, 0.5f, 0.0f); // Alarm color orange!
                } else {
                    ent.color = ent.originalColor;
                }
                
                break;
            }
        }
    }
    
    // Capture & Scoring Collision Logic
    for (auto& agent : m_agents) {
        if (agent.role != ROLE_PREDATOR) continue;
        
        // Find corresponding Entity to check visibility
        bool predVisible = false;
        for (const auto& ent : scene->entities) {
            if (ent.name == agent.entityName) {
                predVisible = ent.visible;
                break;
            }
        }
        if (!predVisible) continue;
        
        // Check collision against prey agents
        for (auto& prey : m_agents) {
            if (prey.role != ROLE_PREY) continue;
            
            // Check if prey entity is active
            Entity* preyEnt = nullptr;
            for (auto& ent : scene->entities) {
                if (ent.name == prey.entityName) {
                    preyEnt = &ent;
                    break;
                }
            }
            if (!preyEnt || !preyEnt->visible) continue;
            
            float dist = glm::distance(agent.position, prey.position);
            if (dist < agent.captureRadius) {
                // Prey captured!
                preyEnt->visible = false;
                preyEnt->hasCollision = false;
                
                if (m_roundState == ROUND_PREDATOR_1_RUN) {
                    m_predator1Score += (int)prey.scoreValue;
                } else if (m_roundState == ROUND_PREDATOR_2_RUN) {
                    m_predator2Score += (int)prey.scoreValue;
                }
                
                std::cout << "[AI System] " << agent.entityName << " CAPTURED " << prey.entityName
                          << " (Value: " << prey.scoreValue << ")" << std::endl;
            }
        }
    }
}

void AISystem::drawDebug(Renderer* renderer, Scene* scene) {
    if (!m_debugEnabled) return;
    
    // Construct local MVP by grabbing camera properties
    glm::mat4 view = scene->camera.GetViewMatrix();
    // Fetch window viewport aspects internally
    glm::mat4 proj = glm::perspective(glm::radians(scene->camera.Zoom), 1920.0f / 1080.0f, 0.1f, 500.0f);
    
    // 1. Draw blocked cells of the navigation grid
    glm::vec3 blockedColor(0.8f, 0.2f, 0.2f);
    for (int cz = 0; cz < m_grid.getDepth(); ++cz) {
        for (int cx = 0; cx < m_grid.getWidth(); ++cx) {
            if (!m_grid.isWalkableCell(cx, cz)) {
                glm::vec3 center = m_grid.cellToWorld(cx, cz);
                center.y = 0.01f; // Slightly above floor
                float hs = m_grid.getCellSize() * 0.45f;
                
                // Draw a small cross to mark cell blockages
                renderer->drawDebugLine(center + glm::vec3(-hs, 0, -hs), center + glm::vec3(hs, 0, hs), blockedColor, view, proj);
                renderer->drawDebugLine(center + glm::vec3(hs, 0, -hs), center + glm::vec3(-hs, 0, hs), blockedColor, view, proj);
            }
        }
    }
    
    // 2. Draw agent sensory cones and paths
    for (const auto& agent : m_agents) {
        // Find corresponding Entity to check visibility
        bool agentActive = false;
        for (const auto& ent : scene->entities) {
            if (ent.name == agent.entityName) {
                agentActive = ent.visible;
                break;
            }
        }
        if (!agentActive) continue;
        
        // A. Draw FOV Vision Cone Boundaries
        glm::vec3 coneColor = (agent.role == ROLE_PREDATOR) ? glm::vec3(1.0f, 0.5f, 0.5f) : glm::vec3(0.5f, 1.0f, 0.5f);
        float hFov = agent.visionFOV * 0.5f;
        
        glm::vec3 forward = agent.forward;
        glm::mat4 rotL = glm::rotate(glm::mat4(1.0f), glm::radians(hFov), glm::vec3(0, 1, 0));
        glm::mat4 rotR = glm::rotate(glm::mat4(1.0f), glm::radians(-hFov), glm::vec3(0, 1, 0));
        
        glm::vec3 leftRay = glm::vec3(rotL * glm::vec4(forward, 0.0f)) * agent.visionRange;
        glm::vec3 rightRay = glm::vec3(rotR * glm::vec4(forward, 0.0f)) * agent.visionRange;
        
        glm::vec3 start = agent.position + glm::vec3(0, 0.1f, 0);
        renderer->drawDebugLine(start, start + leftRay, coneColor, view, proj);
        renderer->drawDebugLine(start, start + rightRay, coneColor, view, proj);
        
        // Connect cone ends to sketch an arc
        int arcSegments = 8;
        glm::vec3 prevPoint = start + leftRay;
        for (int i = 1; i <= arcSegments; ++i) {
            float t = (float)i / (float)arcSegments;
            float angle = glm::radians(hFov - t * agent.visionFOV);
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
            glm::vec3 p = start + glm::vec3(rot * glm::vec4(forward, 0.0f)) * agent.visionRange;
            renderer->drawDebugLine(prevPoint, p, coneColor, view, proj);
            prevPoint = p;
        }
        
        // B. Draw Active Path waypoints
        if (!agent.path.empty()) {
            glm::vec3 pathColor = (agent.role == ROLE_PREDATOR) ? glm::vec3(1.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 0.8f, 0.8f);
            glm::vec3 prevWp = agent.position + glm::vec3(0, 0.05f, 0);
            for (size_t i = agent.currentWaypointIndex; i < agent.path.size(); ++i) {
                glm::vec3 wp = agent.path[i] + glm::vec3(0, 0.05f, 0);
                renderer->drawDebugLine(prevWp, wp, pathColor, view, proj);
                
                // Draw tiny waypoint markers
                float ms = 0.15f;
                renderer->drawDebugLine(wp - glm::vec3(ms, 0, 0), wp + glm::vec3(ms, 0, 0), pathColor, view, proj);
                renderer->drawDebugLine(wp - glm::vec3(0, 0, ms), wp + glm::vec3(0, 0, ms), pathColor, view, proj);
                
                prevWp = wp;
            }
        }
        
        // C. Draw active target chase line
        if (agent.role == ROLE_PREDATOR && agent.state == CHASE) {
            for (const auto& other : m_agents) {
                if (other.entityName == agent.targetEntityName) {
                    renderer->drawDebugLine(agent.position + glm::vec3(0, 0.4f, 0), other.position + glm::vec3(0, 0.4f, 0), glm::vec3(1.0f, 0.9f, 0.0f), view, proj);
                    break;
                }
            }
        }
    }
}

bool AISystem::getActivePredatorInfo(glm::vec3& outPos, glm::vec3& outForward) const {
    std::string targetName = (m_roundState == ROUND_PREDATOR_2_RUN) ? "Predator_2" : "Predator_1";
    for (const auto& agent : m_agents) {
        if (agent.entityName == targetName) {
            outPos = agent.position;
            outForward = agent.forward;
            return true;
        }
    }
    return false;
}
