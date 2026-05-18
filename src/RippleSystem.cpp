#include "RippleSystem.h"
#include "ResourceManager.h"
#include "Shader.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace {
struct GPURippleImpulse {
    glm::vec4 positionRadius;
    glm::vec4 strength;
};

constexpr int MAX_RIPPLE_IMPULSES = 128;
}

RippleSystem& RippleSystem::instance() {
    static RippleSystem system;
    return system;
}

void RippleSystem::initialize(int resolution, glm::vec2 origin, float sizeMeters) {
    if (initialized) return;

    gridResolution = resolution;
    worldOrigin = origin;
    worldSize = sizeMeters;

    glGenTextures(3, heightTextures);
    for (unsigned int tex : heightTextures) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, gridResolution, gridResolution, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        float clearValue = 0.0f;
        glClearTexImage(tex, 0, GL_RED, GL_FLOAT, &clearValue);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &impulseSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, impulseSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_RIPPLE_IMPULSES * sizeof(GPURippleImpulse), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    initialized = true;
}

void RippleSystem::reset() {
    if (!initialized) return;

    float clearValue = 0.0f;
    for (unsigned int tex : heightTextures) {
        glClearTexImage(tex, 0, GL_RED, GL_FLOAT, &clearValue);
    }
    debugImpulses.clear();
}

void RippleSystem::update(float deltaTime, const std::vector<RippleImpulse>& impulses) {
    initialize();

    Shader* injectShader = ResourceManager::getShader("rippleInject");
    Shader* stepShader = ResourceManager::getShader("rippleStep");

    debugImpulses.clear();
    std::vector<GPURippleImpulse> gpuImpulses;
    gpuImpulses.reserve(std::min<int>((int)impulses.size(), MAX_RIPPLE_IMPULSES));
    float impulseEnergy = 0.0f;

    for (const RippleImpulse& impulse : impulses) {
        if (gpuImpulses.size() >= MAX_RIPPLE_IMPULSES) break;
        if (impulse.radius <= 0.001f || !std::isfinite(impulse.strength)) continue;

        RippleImpulse clamped = impulse;
        clamped.radius = glm::clamp(clamped.radius, 0.10f, 1.5f);
        clamped.strength = glm::clamp(clamped.strength * params.rippleAmplitude, -0.080f, 0.035f);
        float energyCost = glm::abs(clamped.strength) * clamped.radius * clamped.radius;
        if (energyCost <= 0.00001f) continue;
        float energyBudget = 0.22f * glm::clamp(params.rippleAmplitude, 0.2f, 1.3f);
        if (impulseEnergy + energyCost > energyBudget) {
            float remaining = glm::max(0.0f, energyBudget - impulseEnergy);
            if (remaining <= 0.0001f) break;
            clamped.strength *= remaining / energyCost;
            energyCost = remaining;
        }
        impulseEnergy += energyCost;
        debugImpulses.push_back(clamped);

        GPURippleImpulse gpu{};
        gpu.positionRadius = glm::vec4(clamped.position, clamped.radius);
        gpu.strength = glm::vec4(clamped.strength, 0.0f, 0.0f, 0.0f);
        gpuImpulses.push_back(gpu);
    }

    if (injectShader && !gpuImpulses.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, impulseSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, gpuImpulses.size() * sizeof(GPURippleImpulse), gpuImpulses.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, impulseSSBO);

        injectShader->use();
        injectShader->setInt("u_impulseCount", (int)gpuImpulses.size());
        injectShader->setInt("u_resolution", gridResolution);
        injectShader->setVec2("u_origin", worldOrigin);
        injectShader->setFloat("u_worldSize", worldSize);
        injectShader->setFloat("u_maxRippleHeight", glm::clamp(params.maxRippleHeight, 0.04f, 0.10f));
        glBindImageTexture(0, heightTextures[currentIndex], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        glBindImageTexture(1, heightTextures[previousIndex], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        int groups = (gridResolution + 15) / 16;
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    if (stepShader) {
        float dt = glm::clamp(deltaTime, 0.0f, 0.033f);
        float waveSpeed = params.ripplePropagationSpeed;
        float texel = texelWorldSize();
        float cfl = glm::clamp((waveSpeed * dt / texel) * (waveSpeed * dt / texel), 0.0f, 0.44f);

        stepShader->use();
        stepShader->setInt("u_resolution", gridResolution);
        stepShader->setFloat("u_cfl", cfl);
        stepShader->setFloat("u_damping", params.rippleDamping);
        stepShader->setFloat("u_amplitudeClamp", glm::clamp(params.maxRippleHeight, 0.04f, 0.10f));
        glBindImageTexture(0, heightTextures[previousIndex], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(1, heightTextures[currentIndex], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(2, heightTextures[nextIndex], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        int groups = (gridResolution + 15) / 16;
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        previousIndex = currentIndex;
        currentIndex = nextIndex;
        nextIndex = 3 - previousIndex - currentIndex;
    }
}

void RippleSystem::bindRippleUniforms(const Shader& shader, int textureUnit, bool enableRipples) {
    RippleSystem& ripple = instance();
    ripple.initialize();

    shader.setBool("u_useRipple", enableRipples);
    shader.setInt("rippleHeightTex", textureUnit);
    shader.setVec2("u_rippleOrigin", ripple.origin());
    shader.setFloat("u_rippleWorldSize", ripple.worldSizeMeters());
    shader.setFloat("u_rippleTexelWorldSize", ripple.texelWorldSize());
    shader.setFloat("u_visualRippleScale", ripple.params.visualRippleScale);
    shader.setFloat("u_physicsRippleScale", ripple.params.physicsRippleScale);
    shader.setFloat("u_rippleNormalStrength", ripple.params.rippleNormalStrength);
    shader.setFloat("u_reflectionStrength", ripple.params.reflectionStrength);
    shader.setFloat("u_fresnelStrength", ripple.params.fresnelStrength);
    shader.setFloat("u_waveSteepness", ripple.params.waveSteepness);
    shader.setFloat("u_waterNormalStrength", ripple.params.waterNormalStrength);

    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, ripple.currentTexture());
}
