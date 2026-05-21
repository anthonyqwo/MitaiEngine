#ifndef AI_SYSTEM_H
#define AI_SYSTEM_H

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "Entity.h"
#include "NavigationGrid.h"

enum AIRole {
    ROLE_PREY,
    ROLE_PREDATOR
};

enum AISpecies {
    SPECIES_GREEN_PREY,
    SPECIES_BLUE_PREY,
    SPECIES_PREDATOR_1,
    SPECIES_PREDATOR_2
};

enum AIState {
    WANDER,
    FLEE,
    SEARCH,
    CHASE
};

struct AIAgent {
    std::string entityName;
    AIRole role;
    AISpecies species;
    AIState state;
    
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 velocity;
    float maxSpeed;
    float acceleration;
    
    float visionRange;
    float visionFOV; // in degrees
    float captureRadius;
    float scoreValue; // for prey
    
    std::string targetEntityName; // current target for predator chase
    glm::vec3 targetPosition;     // seek target position
    std::vector<glm::vec3> path;  // A* path waypoints
    size_t currentWaypointIndex;
    
    float wanderTimer;  // timer to choose new wander destination
    float repathTimer;  // throttle A* calls
    
    float velocityY;
    bool isGrounded;
    
    // Stuck tracking
    float stuckTimer;
    glm::vec3 lastPosition;
};

enum SimRoundState {
    ROUND_PREDATOR_1_RUN,
    ROUND_PREDATOR_1_FINISHED,
    ROUND_PREDATOR_2_RUN,
    ROUND_SIMULATION_FINISHED
};

class AISystem {
public:
    static AISystem& instance();
    
    void initializeGrid(float minX, float maxX, float minZ, float maxZ, float cellSize);
    void setupScene(class Scene* scene);
    void update(class Scene* scene, float deltaTime);
    
    void resetSimulation(class Scene* scene);
    void startNextRound(class Scene* scene);
    void forceResetAll(class Scene* scene);
    
    // Debugging utilities
    void drawDebug(class Renderer* renderer, class Scene* scene);
    bool isDebugEnabled() const { return m_debugEnabled; }
    void setDebugEnabled(bool enable) { m_debugEnabled = enable; }
    
    // ImGui Controls & Parameters
    float getRoundDuration() const { return m_roundDuration; }
    void setRoundDuration(float duration) { m_roundDuration = duration; }
    int getSeed() const { return m_seed; }
    void setSeed(int seed) { m_seed = seed; }
    bool isDeterministic() const { return m_deterministic; }
    void setDeterministic(bool det) { m_deterministic = det; }
    
    // Live round stats
    SimRoundState getRoundState() const { return m_roundState; }
    float getRemainingTime() const { return m_remainingTime; }
    int getPredator1Score() const { return m_predator1Score; }
    int getPredator2Score() const { return m_predator2Score; }
    
    // Predator Perspective Cam Helper
    bool getActivePredatorInfo(glm::vec3& outPos, glm::vec3& outForward) const;
    
    // Global parameters for tuning
    float preyGreenSpeedCoef;
    float preyBlueSpeedCoef;
    float predatorSpeedCoef;
    
    float preyGreenVisionRange;
    float preyBlueVisionRange;
    float predator1VisionRange;
    float predator2VisionRange;
    
private:
    AISystem();
    ~AISystem();
    
    void updateSensor(AIAgent& agent, const class Scene* scene);
    void updateFSM(AIAgent& agent, float deltaTime, class Scene* scene);
    void updateMovement(AIAgent& agent, float deltaTime, class Scene* scene);
    
    bool hasLineOfSight(glm::vec3 start, glm::vec3 end, const class Scene* scene) const;
    
    void spawnPrey(class Scene* scene);
    void spawnPredator(class Scene* scene, AISpecies species);
    
    void syncAgentsToEntities(class Scene* scene);
    
    NavigationGrid m_grid;
    std::vector<AIAgent> m_agents;
    
    // Simulation properties
    SimRoundState m_roundState;
    float m_roundDuration;
    float m_remainingTime;
    int m_predator1Score;
    int m_predator2Score;
    
    int m_seed;
    bool m_deterministic;
    bool m_debugEnabled;
};

#endif
