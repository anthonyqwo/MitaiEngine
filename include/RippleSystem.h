#ifndef RIPPLE_SYSTEM_H
#define RIPPLE_SYSTEM_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class Shader;

struct RippleImpulse {
    glm::vec3 position;
    float radius;
    float strength;
};

struct RippleTuning {
    float rippleAmplitude = 1.0f;
    float visualRippleScale = 1.35f;
    float physicsRippleScale = 0.15f;
    float rippleDamping = 0.990f;
    float ripplePropagationSpeed = 3.85f;
    float maxRippleHeight = 0.08f;
    float reflectionStrength = 1.28f;
    float fresnelStrength = 1.14f;
    float waveSteepness = 1.0f;
    float rippleNormalStrength = 1.10f;
    float waterNormalStrength = 1.05f;
};

class RippleSystem {
public:
    static RippleSystem& instance();

    void initialize(int resolution = 256, glm::vec2 origin = glm::vec2(-5.0f), float worldSize = 10.0f);
    void reset();
    void update(float deltaTime, const std::vector<RippleImpulse>& impulses);

    unsigned int currentTexture() const { return heightTextures[currentIndex]; }
    int resolution() const { return gridResolution; }
    glm::vec2 origin() const { return worldOrigin; }
    float worldSizeMeters() const { return worldSize; }
    float texelWorldSize() const { return worldSize / float(gridResolution); }
    const std::vector<RippleImpulse>& lastImpulses() const { return debugImpulses; }
    RippleTuning& tuning() { return params; }
    const RippleTuning& tuning() const { return params; }

    static void bindRippleUniforms(const Shader& shader, int textureUnit = 9, bool enableRipples = true);

private:
    RippleSystem() = default;

    unsigned int heightTextures[3] = {0, 0, 0};
    unsigned int impulseSSBO = 0;
    int currentIndex = 0;
    int previousIndex = 1;
    int nextIndex = 2;
    int gridResolution = 0;
    glm::vec2 worldOrigin = glm::vec2(-5.0f);
    float worldSize = 10.0f;
    RippleTuning params;
    bool initialized = false;
    std::vector<RippleImpulse> debugImpulses;
};

#endif
