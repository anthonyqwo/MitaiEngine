#define GLM_ENABLE_EXPERIMENTAL
#include "PhysicsSystem.h"
#include "WaterWaves.h"
#include "RippleSystem.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "ResourceManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_map>

int g_collisionChecks = 0;

namespace {
std::unordered_map<const Entity*, glm::vec3> g_previousBuoyantPositions;
std::unordered_map<const Entity*, std::vector<float>> g_filteredBuoyancyHeights;

constexpr float kRippleContactDepthThreshold = 0.010f;
constexpr float kRippleDebugPressureScale = 1000.0f * 9.81f * 0.75f;

glm::vec3 clampVector(glm::vec3 v, float maxLength) {
    float len = glm::length(v);
    if (!std::isfinite(len)) return glm::vec3(0.0f);
    if (len <= maxLength) return v;
    return (v / len) * maxLength;
}

void addRippleImpulse(std::vector<RippleImpulse>& impulses, glm::vec3 position, float radius, float strength) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(strength)) return;
    if (position.x < -5.8f || position.x > 5.8f || position.z < -5.8f || position.z > 5.8f) return;

    RippleImpulse impulse;
    impulse.position = position;
    impulse.radius = glm::clamp(radius, 0.10f, 1.5f);
    impulse.strength = glm::clamp(strength, -0.080f, 0.035f);
    impulses.push_back(impulse);
}

void addSurfaceRippleImpulse(Entity& e,
                             std::vector<RippleImpulse>& impulses,
                             glm::vec3 position,
                             float radius,
                             float strength,
                             float pressure) {
    addRippleImpulse(impulses, position, radius, strength);
    e.debugRippleInjectionPoints.push_back(glm::vec3(position.x, 4.0f, position.z));
    e.debugRippleInjectionRadii.push_back(glm::clamp(radius, 0.10f, 1.5f));
    e.debugRippleInjectionPressures.push_back(glm::max(pressure, 0.0f));
    e.debugRippleInjectionStrengths.push_back(glm::clamp(strength, -0.080f, 0.035f));
}

struct BuoyantBox {
    glm::vec3 center;
    glm::vec3 halfExtents;
    glm::vec3 axes[3];
};

BuoyantBox makeBuoyantBox(const Entity& e) {
    BuoyantBox box;
    box.center = e.position;
    box.halfExtents = glm::max(e.scale * 0.5f, glm::vec3(0.001f));
    glm::mat3 R = glm::mat3_cast(e.orientation);
    box.axes[0] = glm::normalize(R[0]);
    box.axes[1] = glm::normalize(R[1]);
    box.axes[2] = glm::normalize(R[2]);
    return box;
}

float inverseMass(const Entity& e) {
    return (e.mass > 0.001f) ? (1.0f / e.mass) : 0.0f;
}

void applyBuoyantCollisionResponse(Entity& a, Entity& b, glm::vec3 normal, float penetration) {
    if (penetration <= 0.0f || glm::dot(normal, normal) < 0.000001f) return;

    normal = glm::normalize(normal);
    float invMassA = inverseMass(a);
    float invMassB = inverseMass(b);
    float invMassSum = invMassA + invMassB;
    if (invMassSum <= 0.0f) return;

    constexpr float slop = 0.002f;
    float correctionDepth = glm::max(penetration - slop, 0.0f);
    glm::vec3 correction = normal * (correctionDepth / invMassSum);
    a.position += correction * invMassA;
    b.position -= correction * invMassB;

    glm::vec3 relVel = a.velocity - b.velocity;
    float velAlongNormal = glm::dot(relVel, normal);
    if (velAlongNormal < 0.0f) {
        float restitution = 0.18f;
        float impulseMag = -(1.0f + restitution) * velAlongNormal / invMassSum;
        glm::vec3 impulse = impulseMag * normal;
        a.velocity += impulse * invMassA;
        b.velocity -= impulse * invMassB;
    }

    glm::vec3 tangentVel = relVel - glm::dot(relVel, normal) * normal;
    float tangentLen = glm::length(tangentVel);
    if (tangentLen > 0.0001f) {
        glm::vec3 tangent = tangentVel / tangentLen;
        float frictionMag = glm::min(tangentLen / invMassSum, penetration * 2.0f);
        glm::vec3 frictionImpulse = -0.18f * frictionMag * tangent;
        a.velocity += frictionImpulse * invMassA;
        b.velocity -= frictionImpulse * invMassB;
    }
}

bool sphereVsBuoyantBox(const Entity& sphere, const Entity& boxEntity, glm::vec3& normal, float& penetration) {
    BuoyantBox box = makeBuoyantBox(boxEntity);
    glm::vec3 toSphere = sphere.position - box.center;
    glm::vec3 local(
        glm::dot(toSphere, box.axes[0]),
        glm::dot(toSphere, box.axes[1]),
        glm::dot(toSphere, box.axes[2])
    );
    glm::vec3 closestLocal = glm::clamp(local, -box.halfExtents, box.halfExtents);
    glm::vec3 deltaLocal = local - closestLocal;
    float distSq = glm::dot(deltaLocal, deltaLocal);
    float radius = sphere.radius;

    if (distSq >= radius * radius) return false;

    if (distSq > 0.000001f) {
        float dist = std::sqrt(distSq);
        normal = (box.axes[0] * deltaLocal.x + box.axes[1] * deltaLocal.y + box.axes[2] * deltaLocal.z) / dist;
        penetration = radius - dist;
        return true;
    }

    float faceDistances[6] = {
        local.x + box.halfExtents.x,
        box.halfExtents.x - local.x,
        local.y + box.halfExtents.y,
        box.halfExtents.y - local.y,
        local.z + box.halfExtents.z,
        box.halfExtents.z - local.z
    };
    glm::vec3 faceNormals[6] = {
        -box.axes[0], box.axes[0],
        -box.axes[1], box.axes[1],
        -box.axes[2], box.axes[2]
    };

    int closestFace = 0;
    for (int i = 1; i < 6; ++i) {
        if (faceDistances[i] < faceDistances[closestFace]) {
            closestFace = i;
        }
    }
    normal = faceNormals[closestFace];
    penetration = radius + glm::max(faceDistances[closestFace], 0.0f);
    return true;
}

bool buoyantBoxVsBox(const Entity& aEntity, const Entity& bEntity, glm::vec3& normal, float& penetration) {
    BuoyantBox a = makeBuoyantBox(aEntity);
    BuoyantBox b = makeBuoyantBox(bEntity);

    glm::mat3 R(1.0f);
    glm::mat3 AbsR(1.0f);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = glm::dot(a.axes[i], b.axes[j]);
            AbsR[i][j] = std::abs(R[i][j]) + 0.0001f;
        }
    }

    glm::vec3 centerDelta = b.center - a.center;
    glm::vec3 t(glm::dot(centerDelta, a.axes[0]), glm::dot(centerDelta, a.axes[1]), glm::dot(centerDelta, a.axes[2]));
    float bestOverlap = std::numeric_limits<float>::max();
    glm::vec3 bestNormal(1.0f, 0.0f, 0.0f);

    auto testAxis = [&](glm::vec3 axis, float overlap) -> bool {
        if (overlap < 0.0f) return false;
        if (overlap < bestOverlap && glm::dot(axis, axis) > 0.000001f) {
            axis = glm::normalize(axis);
            if (glm::dot(axis, a.center - b.center) < 0.0f) axis = -axis;
            bestOverlap = overlap;
            bestNormal = axis;
        }
        return true;
    };

    float aExt[3] = { a.halfExtents.x, a.halfExtents.y, a.halfExtents.z };
    float bExt[3] = { b.halfExtents.x, b.halfExtents.y, b.halfExtents.z };
    float tVals[3] = { t.x, t.y, t.z };

    for (int i = 0; i < 3; ++i) {
        float ra = aExt[i];
        float rb = bExt[0] * AbsR[i][0] + bExt[1] * AbsR[i][1] + bExt[2] * AbsR[i][2];
        if (!testAxis(a.axes[i], ra + rb - std::abs(tVals[i]))) return false;
    }

    for (int j = 0; j < 3; ++j) {
        float ra = aExt[0] * AbsR[0][j] + aExt[1] * AbsR[1][j] + aExt[2] * AbsR[2][j];
        float rb = bExt[j];
        float dist = std::abs(tVals[0] * R[0][j] + tVals[1] * R[1][j] + tVals[2] * R[2][j]);
        if (!testAxis(b.axes[j], ra + rb - dist)) return false;
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            glm::vec3 axis = glm::cross(a.axes[i], b.axes[j]);
            if (glm::dot(axis, axis) < 0.000001f) continue;

            int i1 = (i + 1) % 3;
            int i2 = (i + 2) % 3;
            int j1 = (j + 1) % 3;
            int j2 = (j + 2) % 3;
            float ra = aExt[i1] * AbsR[i2][j] + aExt[i2] * AbsR[i1][j];
            float rb = bExt[j1] * AbsR[i][j2] + bExt[j2] * AbsR[i][j1];
            float dist = std::abs(tVals[i2] * R[i1][j] - tVals[i1] * R[i2][j]);
            if (!testAxis(axis, ra + rb - dist)) return false;
        }
    }

    normal = bestNormal;
    penetration = bestOverlap;
    return true;
}
}

void PhysicsSystem::reset() {
    gridBuilt = false;
    g_collisionChecks = 0;
    g_previousBuoyantPositions.clear();
    g_filteredBuoyancyHeights.clear();
    RippleSystem::instance().reset();
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

void PhysicsSystem::update(Scene* scene, float deltaTime, bool useGrid, bool waterWavesEnabled) {
    g_collisionChecks = 0;

    std::vector<RippleImpulse> rippleImpulses;
    for (auto& e : scene->entities) {
        if (!e.isBuoyant || !e.visible) continue;

        e.debugRippleInjectionPoints.clear();
        e.debugRippleInjectionRadii.clear();
        e.debugRippleInjectionPressures.clear();
        e.debugRippleInjectionStrengths.clear();

        glm::vec3 previousPos = e.position;
        auto previousIt = g_previousBuoyantPositions.find(&e);
        if (previousIt != g_previousBuoyantPositions.end()) {
            previousPos = previousIt->second;
        }
        glm::vec3 frameVelocity = (deltaTime > 0.0001f) ? (e.position - previousPos) / deltaTime : glm::vec3(0.0f);
        glm::vec3 rippleVelocity = e.velocity;
        if (glm::length2(frameVelocity) > glm::length2(rippleVelocity)) {
            rippleVelocity = frameVelocity;
        }
        glm::vec2 horizontalVelocity(rippleVelocity.x, rippleVelocity.z);
        float horizontalSpeed = glm::length(horizontalVelocity);
        float contactDepth = 0.0f;
        if (e.buoyancyType == 0) {
            contactDepth = 4.0f - (e.position.y - e.radius);
        } else {
            glm::vec3 bottomLocal(0.0f, -e.scale.y * 0.5f, 0.0f);
            glm::vec3 bottomWorld = e.position + e.orientation * bottomLocal;
            contactDepth = 4.0f - bottomWorld.y;
        }
        float massScale = glm::clamp(e.mass / 8.0f, 0.20f, 1.40f);
        float submerged = glm::clamp(e.submergedFraction, 0.0f, 1.0f);
        float contactDepth01 = glm::clamp((contactDepth - kRippleContactDepthThreshold) / 0.35f, 0.0f, 1.0f);

        if (contactDepth > kRippleContactDepthThreshold) {
            if (e.buoyancyType == 0) {
                float patchRadius = glm::clamp(e.radius * 0.42f, 0.16f, 0.42f);
                float contactMotion = 0.18f + glm::clamp(horizontalSpeed, 0.0f, 1.5f) * 0.22f + glm::clamp(-rippleVelocity.y, 0.0f, 0.8f) * 0.28f;
                float totalStrength = -0.0016f * (0.25f + contactDepth01) * contactMotion * massScale;
                for (int i = 0; i < 8; ++i) {
                    float a = (float(i) / 8.0f) * glm::two_pi<float>();
                    glm::vec3 contactPos(e.position.x + glm::cos(a) * patchRadius * 0.70f, 4.0f, e.position.z + glm::sin(a) * patchRadius * 0.70f);
                    addSurfaceRippleImpulse(e, rippleImpulses, contactPos, patchRadius, totalStrength / 8.0f, contactDepth01 * kRippleDebugPressureScale);
                }
            }
        }

        if (horizontalSpeed > 0.18f && contactDepth > kRippleContactDepthThreshold) {
            glm::vec3 direction(horizontalVelocity.x, 0.0f, horizontalVelocity.y);
            direction /= glm::max(horizontalSpeed, 0.001f);
            glm::vec3 side(-direction.z, 0.0f, direction.x);
            float patchRadius = (e.buoyancyType == 0)
                ? glm::clamp(e.radius * 0.45f, 0.18f, 0.42f)
                : glm::clamp(glm::max(e.scale.x, e.scale.z) * 0.16f, 0.22f, 0.55f);
            float strength = -0.0075f * horizontalSpeed * (0.18f + contactDepth01) * massScale;
            glm::vec3 wakeCenter = e.position - direction * patchRadius;
            for (int i = -1; i <= 1; ++i) {
                glm::vec3 wakePos = wakeCenter + side * (float(i) * patchRadius * 0.55f);
                addSurfaceRippleImpulse(e, rippleImpulses, wakePos, patchRadius, strength / 3.0f, glm::abs(strength) * kRippleDebugPressureScale);
            }
        }

        if (rippleVelocity.y < -0.5f && contactDepth > kRippleContactDepthThreshold && contactDepth < 1.5f) {
            float strength = 0.026f * rippleVelocity.y * massScale;
            if (e.buoyancyType == 0) {
                float patchRadius = glm::clamp(e.radius * 0.55f, 0.18f, 0.48f);
                for (int i = 0; i < 8; ++i) {
                    float a = (float(i) / 8.0f) * glm::two_pi<float>();
                    glm::vec3 contactPos(e.position.x + glm::cos(a) * patchRadius * 0.55f, 4.0f, e.position.z + glm::sin(a) * patchRadius * 0.55f);
                    addSurfaceRippleImpulse(e, rippleImpulses, contactPos, patchRadius, strength / 8.0f, glm::abs(strength) * kRippleDebugPressureScale);
                }
            } else {
                glm::vec3 bottomLocal(0.0f, -e.scale.y * 0.5f, 0.0f);
                glm::vec3 contactPos = e.position + e.orientation * bottomLocal;
                addSurfaceRippleImpulse(e, rippleImpulses, contactPos, 0.30f, strength, glm::abs(strength) * kRippleDebugPressureScale);
            }
        }

        if (e.isGrabbed && contactDepth > kRippleContactDepthThreshold && submerged > 0.03f) {
            float grabSpeed = glm::length(e.targetGrabWorld - (e.position + e.orientation * e.localGrabOffset));
            float strength = -0.010f * glm::clamp(grabSpeed, 0.0f, 2.5f) * (0.16f + submerged);
            addSurfaceRippleImpulse(e, rippleImpulses, e.position + e.orientation * e.localGrabOffset, 0.28f, strength, glm::abs(strength) * kRippleDebugPressureScale);
        }

        if (e.buoyancyType != 0) {
            constexpr float rho = 1000.0f;
            constexpr float g = 9.81f;
            constexpr int contactGridRes = 6;
            float W = e.scale.x;
            float H = e.scale.y;
            float D = e.scale.z;
            float xh = W * 0.5f;
            float yh = H * 0.5f;
            float zh = D * 0.5f;

            float cellX = W / float(contactGridRes);
            float cellZ = D / float(contactGridRes);
            float baseRadius = glm::clamp(glm::max(cellX, cellZ) * 0.85f, 0.24f, 0.75f);
            float areaWeight = (W * D) / float(contactGridRes * contactGridRes);

            for (int ix = 0; ix < contactGridRes; ++ix) {
                float u = (float(ix) + 0.5f) / float(contactGridRes);
                float lx = glm::mix(-xh, xh, u);
                float edgeX = glm::abs(lx) / glm::max(xh, 0.001f);

                for (int iz = 0; iz < contactGridRes; ++iz) {
                    float v = (float(iz) + 0.5f) / float(contactGridRes);
                    float lz = glm::mix(-zh, zh, v);
                    float edgeZ = glm::abs(lz) / glm::max(zh, 0.001f);
                    glm::vec3 localSample(lx, -yh, lz);
                    glm::vec3 worldSample = e.position + e.orientation * localSample;

                    float depth = 4.0f - worldSample.y;
                    if (depth <= kRippleContactDepthThreshold || depth > 1.25f) continue;

                    glm::vec3 sampleVelocity = rippleVelocity + glm::cross(e.angularVelocity, worldSample - e.position);
                    float horizontalSampleSpeed = glm::length(glm::vec2(sampleVelocity.x, sampleVelocity.z));
                    float impactSpeed = glm::max(-sampleVelocity.y, 0.0f);
                    float pressure = rho * g * depth;
                    float normalizedPressure = glm::clamp(pressure / (rho * g * 0.45f), 0.0f, 1.0f);
                    float motionTerm = 0.018f + horizontalSampleSpeed * 0.09f + impactSpeed * 0.38f;
                    float edgeConcentration = 1.0f + 0.08f * glm::smoothstep(0.45f, 0.95f, glm::max(edgeX, edgeZ));
                    float cornerConcentration = 1.0f;
                    float pressureSign = (impactSpeed > horizontalSampleSpeed * 0.25f) ? -1.0f : -0.55f;
                    float strength = pressureSign * normalizedPressure * motionTerm * areaWeight * massScale * 0.22f * edgeConcentration * cornerConcentration;
                    strength = glm::clamp(strength, -0.050f, 0.0f);

                    float radius = baseRadius + glm::clamp(depth * 0.16f, 0.0f, 0.24f);
                    addSurfaceRippleImpulse(e, rippleImpulses, worldSample, radius, strength, pressure);
                }
            }
        }
    }

    RippleSystem::instance().update(deltaTime, rippleImpulses);
    
    // Gather coordinates for all buoyant queries on the GPU
    std::vector<WaterQuery> queryPoints;
    for (auto& e : scene->entities) {
        if (!e.isBuoyant || !e.visible) continue;
        
        if (e.buoyancyType == 0) { // Sphere
            WaterQuery q;
            q.samplePosition = glm::vec4(e.position, 0.0f);
            q.waterPosition = glm::vec4(e.position.x, 4.0f, e.position.z, 0.0f);
            q.waterNormal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
            q.waterData = glm::vec4(4.0f, 0.0f, 4.0f, 0.0f);
            queryPoints.push_back(q);
        } else { // Slab or Open-top Box
            float W = e.scale.x;
            float H = e.scale.y;
            float D = e.scale.z;
            
            // Flooding corners for Open-top Box (type 2)
            if (e.buoyancyType == 2) {
                float x_h = W * 0.5f;
                float y_t = H * 0.5f;
                float z_h = D * 0.5f;
                glm::vec3 corners[4] = {
                    glm::vec3(-x_h, y_t, -z_h),
                    glm::vec3(x_h, y_t, -z_h),
                    glm::vec3(x_h, y_t, z_h),
                    glm::vec3(-x_h, y_t, z_h)
                };
                for (int i = 0; i < 4; ++i) {
                    glm::vec3 worldC = e.position + e.orientation * corners[i];
                    WaterQuery q;
                    q.samplePosition = glm::vec4(worldC, 0.0f);
                    q.waterPosition = glm::vec4(worldC.x, 4.0f, worldC.z, 0.0f);
                    q.waterNormal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
                    q.waterData = glm::vec4(4.0f, 0.0f, 4.0f, 0.0f);
                    queryPoints.push_back(q);
                }
            }
            
            // Grid voxels
            int gridRes = 6;
            for (int i = 0; i < gridRes; ++i) {
                float lx = -W*0.5f + (float(i) + 0.5f) * (W / float(gridRes));
                for (int j = 0; j < gridRes; ++j) {
                    float ly = -H*0.5f + (float(j) + 0.5f) * (H / float(gridRes));
                    for (int k = 0; k < gridRes; ++k) {
                        float lz = -D*0.5f + (float(k) + 0.5f) * (D / float(gridRes));
                        
                        glm::vec3 worldPos = e.position + e.orientation * glm::vec3(lx, ly, lz);
                        WaterQuery q;
                        q.samplePosition = glm::vec4(worldPos, 0.0f);
                        q.waterPosition = glm::vec4(worldPos.x, 4.0f, worldPos.z, 0.0f);
                        q.waterNormal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
                        q.waterData = glm::vec4(4.0f, 0.0f, 4.0f, 0.0f);
                        queryPoints.push_back(q);
                    }
                }
            }
        }
    }

    std::vector<WaterQuery> waterQueries;
    if (!queryPoints.empty()) {
        static unsigned int querySSBO = 0;
        if (querySSBO == 0) {
            glGenBuffers(1, &querySSBO);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, querySSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, queryPoints.size() * sizeof(WaterQuery), queryPoints.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, querySSBO);
        
        Shader* waveShader = ResourceManager::getShader("waveQuery");
        if (waveShader) {
            waveShader->use();
            waveShader->setInt("numQueries", (int)queryPoints.size());
            waveShader->setFloat("time", (float)glfwGetTime());
            waveShader->setBool("u_enableWaterWaves", waterWavesEnabled);
            setWaterWaveUniforms(*waveShader);
            RippleSystem::bindRippleUniforms(*waveShader, 9, true);
            
            int numGroups = ((int)queryPoints.size() + 255) / 256;
            glDispatchCompute(numGroups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
            
            // Read back data
            waterQueries.resize(queryPoints.size());
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, waterQueries.size() * sizeof(WaterQuery), waterQueries.data());
        } else {
            waterQueries = queryPoints;
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    // Euler integration for velocities
    int queryIndex = 0;

    float cargoMass = 0.0f;
    glm::vec3 cargoOffset(0.0f);
    Entity* openBox = nullptr;
    Entity* boxCargo = nullptr;
    for (auto& e : scene->entities) {
        if (e.name == "Open Box") {
            openBox = &e;
        } else if (e.name == "Box Cargo") {
            boxCargo = &e;
        }
    }
    if (openBox && boxCargo && boxCargo->isCargo) {
        // 1. Get box orientation rotation matrix
        glm::mat3 R = glm::mat3_cast(openBox->orientation);
        
        // 2. Project world gravity (0, -9.81, 0) into local space of the box
        glm::vec3 gravityWorld(0.0f, -9.81f, 0.0f);
        glm::vec3 gravityLocal = glm::transpose(R) * gravityWorld;
        
        // 3. Local sliding velocity and sliding acceleration (including vertical Y gravity)
        glm::vec3 vLocal = boxCargo->velocity; 
        glm::vec3 accelLocal = gravityLocal; // Complete 3D gravity in local space!
        
        // If grabbed by mouse, add spring pull force in local space!
        if (boxCargo->isGrabbed) {
            glm::vec3 targetLocal = glm::transpose(R) * (boxCargo->targetGrabWorld - openBox->position);
            glm::vec3 springForce = 50.0f * (targetLocal - boxCargo->cargoLocalOffset) - 5.0f * vLocal;
            accelLocal += springForce;
        }
        
        vLocal += accelLocal * deltaTime;
        
        // Friction and damping
        float horizontalDamping = glm::clamp(1.0f - 8.0f * deltaTime, 0.0f, 1.0f);
        float verticalDamping = glm::clamp(1.0f - 6.0f * deltaTime, 0.0f, 1.0f);
        vLocal.x *= horizontalDamping;
        vLocal.y *= verticalDamping;
        vLocal.z *= horizontalDamping;
        
        // Update local offset
        boxCargo->cargoLocalOffset += vLocal * deltaTime;
        boxCargo->velocity = vLocal; // Save local velocity
        
        // Constraint bottom floor Y local coordinate (bottom floor surface is at local Y = -0.27m, cargo center must be >= -0.12m)
        if (boxCargo->cargoLocalOffset.y < -0.12f) {
            boxCargo->cargoLocalOffset.y = -0.12f;
            vLocal.y = 0.0f;
            boxCargo->velocity.y = 0.0f;
        }
        
        // Side wall constraints (X & Z limits: inner compartment is 0.54m, cargo center limit is 0.39m)
        float limitX = 0.39f;
        float limitZ = 0.39f;
        
        // If below the wall clearance height (0.45 in Y local), apply collision with side walls
        if (boxCargo->cargoLocalOffset.y < 0.45f) {
            if (glm::abs(boxCargo->cargoLocalOffset.x) > limitX) {
                boxCargo->cargoLocalOffset.x = (boxCargo->cargoLocalOffset.x > 0.0f ? 1.0f : -1.0f) * limitX;
                vLocal.x = 0.0f;
                boxCargo->velocity.x = 0.0f;
            }
            if (glm::abs(boxCargo->cargoLocalOffset.z) > limitZ) {
                boxCargo->cargoLocalOffset.z = (boxCargo->cargoLocalOffset.z > 0.0f ? 1.0f : -1.0f) * limitZ;
                vLocal.z = 0.0f;
                boxCargo->velocity.z = 0.0f;
            }
        }
        
        // 4. CHECK UNPARENTING / FALLING OUT TRIGGER!
        // If cargo Y > 0.45m (escaped opening fully)
        // or if the box is inverted (gravityLocal.y > 0, pulling out) and the cargo has left the bottom floor (Y > -0.10m)
        bool fallOut = false;
        if (boxCargo->cargoLocalOffset.y > 0.45f) {
            fallOut = true;
        } else if (gravityLocal.y > 4.0f && boxCargo->cargoLocalOffset.y > 0.15f) {
            fallOut = true;
        }
        
        if (fallOut) {
            // Unparent! Convert to standalone floating buoyant block!
            boxCargo->isCargo = false;
            boxCargo->isBuoyant = true;
            boxCargo->hasCollision = true;
            
            // Calculate starting world position and velocity
            glm::vec3 worldPos = openBox->position + openBox->orientation * boxCargo->cargoLocalOffset;
            glm::vec3 worldVel = openBox->velocity + glm::cross(openBox->angularVelocity, worldPos - openBox->position);
            
            boxCargo->position = worldPos;
            boxCargo->velocity = worldVel + openBox->orientation * glm::vec3(0.0f, 1.5f, 0.0f); // Add pop eject impulse
            boxCargo->angularVelocity = openBox->angularVelocity;
        } else {
            // Parent tracking update
            boxCargo->position = openBox->position + openBox->orientation * boxCargo->cargoLocalOffset;
            boxCargo->orientation = openBox->orientation;
            
            cargoMass = boxCargo->mass;
            cargoOffset = boxCargo->cargoLocalOffset;
        }
    }

    for (auto& e : scene->entities) {
        if (e.isBuoyant) {
            if (e.name == "Open Box") {
                updateBuoyancy(e, deltaTime, waterQueries, queryIndex, cargoMass, cargoOffset);
            } else {
                updateBuoyancy(e, deltaTime, waterQueries, queryIndex);
            }
        } else if (e.name == "DynamicSphere") {
            e.velocity.y -= 9.81f * deltaTime; // gravity
            e.position += e.velocity * deltaTime;
        }
    }

    resolveBuoyantBodyCollisions(scene);
    
    if (useGrid) {
        if (!gridBuilt) buildStaticGrid(scene);
        updateGrid(scene, deltaTime);
    } else {
        updateExhaustive(scene, deltaTime);
    }

    for (auto& e : scene->entities) {
        if (e.isBuoyant && e.visible) {
            g_previousBuoyantPositions[&e] = e.position;
        }
    }
}

void PhysicsSystem::updateBuoyancy(Entity& e, float deltaTime, const std::vector<PhysicsSystem::WaterQuery>& waterQueries, int& queryIndex, float cargoMass, glm::vec3 cargoOffset) {
    // Cap deltaTime to prevent a large frame from injecting a physics spike.
    if (deltaTime > 0.05f) deltaTime = 0.05f;
    if (deltaTime <= 0.0f) return;

    float yw = 4.0f; // Default water plane fallback
    float rho = 1000.0f;
    float g = 9.81f;
    e.debugBuoyancySamples.clear();
    e.debugRawWaterSurfacePoints.clear();
    e.debugWaterSurfacePoints.clear();
    e.debugWaterSurfaceNormals.clear();
    e.debugWaterSubmergedDepths.clear();
    std::vector<float>& filteredHeights = g_filteredBuoyancyHeights[&e];
    size_t buoyancySampleIndex = 0;

    auto consumeWaterQuery = [&](const glm::vec3& fallbackSample) {
        WaterQuery q;
        if (!waterQueries.empty() && queryIndex < static_cast<int>(waterQueries.size())) {
            q = waterQueries[queryIndex++];
        } else {
            q.samplePosition = glm::vec4(fallbackSample, 0.0f);
            q.waterPosition = glm::vec4(fallbackSample.x, yw, fallbackSample.z, 0.0f);
            q.waterNormal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
            q.waterData = glm::vec4(yw, 0.0f, yw, 0.0f);
        }

        glm::vec3 rawWaterPoint(q.waterPosition);
        float rawHeight = rawWaterPoint.y;
        if (std::isfinite(q.waterData.z) && q.waterData.z != 0.0f) {
            rawHeight = q.waterData.z;
        }

        if (filteredHeights.size() <= buoyancySampleIndex) {
            filteredHeights.resize(buoyancySampleIndex + 1, rawHeight);
        }
        float previousFiltered = filteredHeights[buoyancySampleIndex];
        float filterAlpha = glm::clamp(deltaTime * 7.5f, 0.08f, 0.26f);
        float filteredHeight = glm::mix(previousFiltered, rawHeight, filterAlpha);
        filteredHeights[buoyancySampleIndex] = filteredHeight;
        q.waterPosition.y = filteredHeight;
        q.waterData.z = filteredHeight;
        q.waterData.y = filteredHeight - q.waterData.x;

        glm::vec3 normal(q.waterNormal);
        if (glm::dot(normal, normal) < 0.0001f) {
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            normal = glm::normalize(normal);
        }
        if (normal.y < 0.0f) normal = -normal;
        normal = glm::normalize(glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), normal, 0.70f));
        q.waterNormal = glm::vec4(normal, 0.0f);

        e.debugBuoyancySamples.push_back(glm::vec3(q.samplePosition));
        e.debugRawWaterSurfacePoints.push_back(rawWaterPoint);
        e.debugWaterSurfacePoints.push_back(glm::vec3(q.waterPosition));
        e.debugWaterSurfaceNormals.push_back(normal);
        e.debugWaterSubmergedDepths.push_back(0.0f);
        ++buoyancySampleIndex;
        return q;
    };

    auto setLastSubmergedDepth = [&](float depth) {
        if (!e.debugWaterSubmergedDepths.empty()) {
            e.debugWaterSubmergedDepths.back() = depth;
        }
    };
    
    // Combined system mass
    float M = e.mass + cargoMass;
    if (M <= 0.001f) return;
    
    // Shift of Center of Mass (CoM) in local space
    glm::vec3 CoM_local = (M > 0.001f) ? (cargoMass / M) * cargoOffset : glm::vec3(0.0f);
    glm::vec3 worldCoM = e.position + e.orientation * CoM_local;

    // 1. Local moment of inertia and total displaced volume.
    glm::vec3 I_local(1.0f);
    float V_total = 1.0f;

    if (e.buoyancyType == 0) { // Hollow Sphere
        float R = e.radius;
        V_total = (4.0f / 3.0f) * glm::pi<float>() * R * R * R;
        I_local = glm::vec3((2.0f / 3.0f) * M * R * R);
    } else if (e.buoyancyType == 1) { // Wooden Slab
        float W = e.scale.x;
        float H = e.scale.y;
        float D = e.scale.z;
        V_total = W * H * D;
        float Ix = (1.0f / 12.0f) * M * (H * H + D * D);
        float Iy = (1.0f / 12.0f) * M * (W * W + D * D);
        float Iz = (1.0f / 12.0f) * M * (W * W + H * H);
        I_local = glm::vec3(Ix, Iy, Iz);
    } else if (e.buoyancyType == 2) { // Open-top Box
        float W = e.scale.x;
        float H = e.scale.y;
        float D = e.scale.z;
        V_total = W * H * D; // outer box volume
        float Ix = (1.0f / 12.0f) * M * (H * H + D * D);
        float Iy = (1.0f / 12.0f) * M * (W * W + D * D);
        float Iz = (1.0f / 12.0f) * M * (W * W + H * H);
        I_local = glm::vec3(Ix, Iy, Iz);
        I_local *= 1.8f;
    }
    I_local = glm::max(I_local, glm::vec3(0.001f));

    // 2. Integrate submerged volume (V_sub) and center of buoyancy (B).
    float V_sub = 0.0f;
    glm::vec3 B(0.0f);
    glm::vec3 normalSum(0.0f);
    float normalWeight = 0.0f;

    if (e.buoyancyType == 0) { // Hollow Sphere
        float R = e.radius;
        WaterQuery water = consumeWaterQuery(e.position);
        glm::vec3 waterPoint(water.waterPosition);
        glm::vec3 queryNormal(water.waterNormal);
        float signedCenterDistance = glm::dot(e.position - waterPoint, queryNormal);
        float h = R - signedCenterDistance; // Submerged depth against the queried Gerstner tangent plane
        setLastSubmergedDepth(h);
        if (h <= 0.0f) {
            V_sub = 0.0f;
            B = e.position;
        } else if (h >= 2.0f * R) {
            V_sub = V_total;
            B = e.position;
        } else {
            V_sub = (glm::pi<float>() * h * h / 3.0f) * (3.0f * R - h);
            float y_max_local = glm::clamp(-signedCenterDistance, -R, R);
            float integral = glm::pi<float>() * ( (R * R * y_max_local * y_max_local) / 2.0f - (y_max_local * y_max_local * y_max_local * y_max_local) / 4.0f - (R * R * R * R) / 4.0f );
            float y_local_buoyancy = integral / V_sub;
            B = e.position + e.orientation * glm::vec3(0.0f, y_local_buoyancy, 0.0f);
        }
        if (V_sub > 0.0f) {
            normalSum += glm::vec3(water.waterNormal) * V_sub;
            normalWeight += V_sub;
        }
        e.submergedFraction = V_sub / V_total;
    } else { // Wooden Slab or Open-top Box
        // Continuous Flooding & Drainage logic with Hysteresis
        bool allCornersAbove = true;
        int numSubmergedCorners = 0;
        float maxRimSubmergedDepth = 0.0f;

        if (e.buoyancyType == 2) {
            float W = e.scale.x;
            float H = e.scale.y;
            float D = e.scale.z;
            float x_h = W * 0.5f;
            float y_t = H * 0.5f;
            float z_h = D * 0.5f;
            glm::vec3 corners[4] = {
                glm::vec3(-x_h, y_t, -z_h),
                glm::vec3(x_h, y_t, -z_h),
                glm::vec3(x_h, y_t, z_h),
                glm::vec3(-x_h, y_t, z_h)
            };

            for (int i = 0; i < 4; ++i) {
                glm::vec3 worldC = e.position + e.orientation * corners[i];
                WaterQuery water = consumeWaterQuery(worldC);
                float submergedDepth = -glm::dot(worldC - glm::vec3(water.waterPosition), glm::vec3(water.waterNormal));
                setLastSubmergedDepth(submergedDepth);
                maxRimSubmergedDepth = glm::max(maxRimSubmergedDepth, submergedDepth);
                float cornerThreshold = (e.floodLevel < 1.0f) ? 0.05f : 0.02f;
                if (submergedDepth > cornerThreshold) {
                    numSubmergedCorners++;
                }
            }
            allCornersAbove = (maxRimSubmergedDepth <= 0.02f);

            // Hysteresis & Accumulation / Drainage update
            if (allCornersAbove) {
                e.dryTimer += deltaTime;
                if (e.dryTimer >= 1.5f) {
                    e.floodLevel -= 0.3f * deltaTime; // drainRate = 0.3f (fully drains in ~3.3s)
                }
            } else {
                e.dryTimer = 0.0f; // Reset timer when wet
                if (numSubmergedCorners >= 2 && maxRimSubmergedDepth > 0.08f) {
                    float ingressRate = glm::clamp((maxRimSubmergedDepth - 0.08f) * 0.7f, 0.0f, 0.18f);
                    e.floodLevel += ingressRate * deltaTime;
                }
            }
            e.floodLevel = glm::clamp(e.floodLevel, 0.0f, 1.0f);
        }

        // Voxel/point integration
        float W = e.scale.x;
        float H = e.scale.y;
        float D = e.scale.z;
        int gridRes = 6;
        float dV = V_total / float(gridRes * gridRes * gridRes);

        glm::vec3 B_sum(0.0f);

        for (int i = 0; i < gridRes; ++i) {
            float lx = -W*0.5f + (float(i) + 0.5f) * (W / float(gridRes));
            for (int j = 0; j < gridRes; ++j) {
                float ly = -H*0.5f + (float(j) + 0.5f) * (H / float(gridRes));
                for (int k = 0; k < gridRes; ++k) {
                    float lz = -D*0.5f + (float(k) + 0.5f) * (D / float(gridRes));

                    float weight = 1.0f;
                    if (e.buoyancyType == 2) {
                        float t = 0.05f; // Wall thickness 5cm
                        bool isWallOrBottom = (ly <= -H*0.5f + t) || 
                                              (glm::abs(lx) >= W*0.5f - t) || 
                                              (glm::abs(lz) >= D*0.5f - t);
                        if (!isWallOrBottom) {
                            weight = glm::max(0.25f, 1.0f - e.floodLevel); // Smoothly interpolate buoyancy!
                        }
                    }

                    glm::vec3 worldPos = e.position + e.orientation * glm::vec3(lx, ly, lz);
                    WaterQuery water = consumeWaterQuery(worldPos);
                    float signedWaterDistance = glm::dot(worldPos - glm::vec3(water.waterPosition), glm::vec3(water.waterNormal));
                    setLastSubmergedDepth(-signedWaterDistance);
                    if (signedWaterDistance < 0.0f) {
                        float effectiveV = dV * weight;
                        V_sub += effectiveV;
                        B_sum += worldPos * effectiveV;
                        normalSum += glm::vec3(water.waterNormal) * effectiveV;
                        normalWeight += effectiveV;
                    }
                }
            }
        }

        if (V_sub > 0.0f) {
            B = B_sum / V_sub;
            e.submergedFraction = V_sub / V_total;
            if (e.submergedFraction > 1.0f) e.submergedFraction = 1.0f;
        } else {
            B = e.position;
            e.submergedFraction = 0.0f;
        }
    }

    // 3. Accumulate gravity, buoyancy, drag, and torque.
    glm::vec3 F_gravity(0.0f, -M * g, 0.0f);
    glm::vec3 waterNormal(0.0f, 1.0f, 0.0f);
    if (normalWeight > 0.0f && glm::dot(normalSum, normalSum) > 0.0001f) {
        waterNormal = glm::normalize(normalSum / normalWeight);
        if (waterNormal.y < 0.0f) waterNormal = -waterNormal;
        waterNormal = glm::normalize(glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), waterNormal, 0.65f));
    }
    e.waterSurfaceNormal = waterNormal;
    glm::vec3 F_buoyant = waterNormal * (rho * g * V_sub);
    float maxBuoyantForce = glm::max(M * g * 4.0f, 1.0f);
    float buoyantLen = glm::length(F_buoyant);
    if (buoyantLen > maxBuoyantForce) {
        F_buoyant = (F_buoyant / buoyantLen) * maxBuoyantForce;
    }
    F_buoyant = glm::mix(e.buoyancyForce, F_buoyant, 0.35f);
    
    // Water drag damps both normal bobbing and tangential drift while submerged.
    float Cd = 4.0f;
    glm::vec3 normalVelocity = waterNormal * glm::dot(e.velocity, waterNormal);
    glm::vec3 tangentialVelocity = e.velocity - normalVelocity;
    glm::vec3 F_drag = -M * ((Cd * e.submergedFraction + 0.1f) * normalVelocity + (1.4f * e.submergedFraction + 0.05f) * tangentialVelocity);
    glm::vec3 F_linearWaterDrag = -e.velocity * (M * (1.8f * e.submergedFraction));
    F_drag += F_linearWaterDrag;
    F_drag = clampVector(F_drag, glm::max(M * g * 2.5f, 1.0f));

    glm::vec3 F_total = F_gravity + F_buoyant + F_drag;

    // Buoyancy torque: center of buoyancy vs. center of mass.
    glm::vec3 torque_buoyancy = glm::cross(B - worldCoM, F_buoyant);

    // Apply interactive soft spring-damper joint constraint if grabbed!
    if (e.isGrabbed) {
        glm::vec3 worldGrabPos = e.position + e.orientation * e.localGrabOffset;
        glm::vec3 vGrab = e.velocity + glm::cross(e.angularVelocity, worldGrabPos - worldCoM);
        
        float Ks = 15000.0f;
        float Kd = 250.0f;
        
        glm::vec3 F_spring = Ks * (e.targetGrabWorld - worldGrabPos) - Kd * vGrab;
        
        float maxForce = glm::max(M * g * 18.0f, 1200.0f);
        float fLen = glm::length(F_spring);
        if (fLen > maxForce) {
            F_spring = (F_spring / fLen) * maxForce;
        }
        
        F_total += F_spring;
        torque_buoyancy += glm::cross(worldGrabPos - worldCoM, F_spring);
    }

    float Dd = (e.buoyancyType == 2) ? 14.0f : 6.0f;
    glm::mat3 R = glm::mat3_cast(e.orientation);
    glm::vec3 omega_local = glm::transpose(R) * e.angularVelocity;
    glm::vec3 L_local = I_local * omega_local;
    glm::vec3 L_world = R * L_local;
    glm::vec3 torque_drag = - (Dd * e.submergedFraction + 0.2f) * L_world;

    glm::vec3 torque_total = torque_buoyancy + torque_drag;

    // 4. Spring-damper penalty collisions with tank walls and bottom.
    float ks = 20000.0f;
    float kd = 300.0f;

    if (e.buoyancyType == 0) { // Sphere Boundaries
        float R = e.radius;
        struct Contact { glm::vec3 pos; glm::vec3 normal; float depth; };
        std::vector<Contact> contacts;
        
        if (worldCoM.y - R < 0.0f) contacts.push_back({ worldCoM + glm::vec3(0.0f, -R, 0.0f), glm::vec3(0, 1, 0), 0.0f - (worldCoM.y - R) });
        if (worldCoM.x - R < -5.0f) contacts.push_back({ worldCoM + glm::vec3(-R, 0.0f, 0.0f), glm::vec3(1, 0, 0), -5.0f - (worldCoM.x - R) });
        if (worldCoM.x + R > 5.0f) contacts.push_back({ worldCoM + glm::vec3(R, 0.0f, 0.0f), glm::vec3(-1, 0, 0), (worldCoM.x + R) - 5.0f });
        if (worldCoM.z - R < -5.0f) contacts.push_back({ worldCoM + glm::vec3(0.0f, 0.0f, -R), glm::vec3(0, 0, 1), -5.0f - (worldCoM.z - R) });
        if (worldCoM.z + R > 5.0f) contacts.push_back({ worldCoM + glm::vec3(0.0f, 0.0f, R), glm::vec3(0, 0, -1), (worldCoM.z + R) - 5.0f });

        for (const auto& c : contacts) {
            glm::vec3 vc = e.velocity + glm::cross(e.angularVelocity, c.pos - worldCoM);
            glm::vec3 Fp = (ks * c.depth) * c.normal - kd * vc;
            Fp = clampVector(Fp, glm::max(M * g * 16.0f, 1000.0f));
            F_total += Fp;
            torque_total += glm::cross(c.pos - worldCoM, Fp);
        }
    } else { // Box Corners (Slab & Open-top Box)
        float W = e.scale.x;
        float H = e.scale.y;
        float D = e.scale.z;
        float x_h = W * 0.5f;
        float y_h = H * 0.5f;
        float z_h = D * 0.5f;
        
        glm::vec3 localCorners[8] = {
            glm::vec3(-x_h, -y_h, -z_h), glm::vec3(x_h, -y_h, -z_h),
            glm::vec3(x_h,  y_h, -z_h), glm::vec3(-x_h,  y_h, -z_h),
            glm::vec3(-x_h, -y_h,  z_h), glm::vec3(x_h, -y_h,  z_h),
            glm::vec3(x_h,  y_h,  z_h), glm::vec3(-x_h,  y_h,  z_h)
        };

        for (int i = 0; i < 8; ++i) {
            glm::vec3 worldC = e.position + e.orientation * localCorners[i];
            
            struct Penalty { glm::vec3 normal; float depth; };
            std::vector<Penalty> penalties;

            if (worldC.y < 0.0f) penalties.push_back({ glm::vec3(0, 1, 0), 0.0f - worldC.y });
            if (worldC.x < -5.0f) penalties.push_back({ glm::vec3(1, 0, 0), -5.0f - worldC.x });
            if (worldC.x > 5.0f) penalties.push_back({ glm::vec3(-1, 0, 0), worldC.x - 5.0f });
            if (worldC.z < -5.0f) penalties.push_back({ glm::vec3(0, 0, 1), -5.0f - worldC.z });
            if (worldC.z > 5.0f) penalties.push_back({ glm::vec3(0, 0, -1), worldC.z - 5.0f });

            if (!penalties.empty()) {
                glm::vec3 vc = e.velocity + glm::cross(e.angularVelocity, worldC - worldCoM);
                for (const auto& p : penalties) {
                    glm::vec3 Fp = ((ks / 8.0f) * p.depth) * p.normal - (kd / 8.0f) * vc;
                    Fp = clampVector(Fp, glm::max(M * g * 6.0f, 500.0f));
                    F_total += Fp;
                    torque_total += glm::cross(worldC - worldCoM, Fp);
                }
            }
        }
    }

    F_total = clampVector(F_total, glm::max(M * g * 8.0f, 1.0f));
    float maxTorque = glm::max(M * g * glm::length(e.scale) * 2.5f, 20.0f);
    torque_total = clampVector(torque_total, maxTorque);

    glm::vec3 accel = F_total / M;
    e.velocity += accel * deltaTime;
    e.velocity = clampVector(e.velocity, 12.0f);
    worldCoM += e.velocity * deltaTime;

    glm::vec3 torque_local = glm::transpose(R) * torque_total;
    glm::vec3 alpha_local = torque_local / I_local;
    glm::vec3 alpha_world = R * alpha_local;
    e.angularVelocity += alpha_world * deltaTime;
    float angularDamping = std::pow(0.985f, deltaTime * 60.0f);
    e.angularVelocity *= angularDamping;

    // Safety Angular Speed Clamp
    float angSpeed = glm::length(e.angularVelocity);
    float maxAngSpeed = (e.buoyancyType == 2) ? 4.0f : 8.0f;
    if (angSpeed > maxAngSpeed) {
        e.angularVelocity = (e.angularVelocity / angSpeed) * maxAngSpeed;
    }

    glm::quat w_quat(0.0f, e.angularVelocity.x, e.angularVelocity.y, e.angularVelocity.z);
    e.orientation += (0.5f * w_quat * e.orientation) * deltaTime;
    e.orientation = glm::normalize(e.orientation);

    // Update entity geometric position from updated worldCoM
    e.position = worldCoM - e.orientation * CoM_local;

    e.buoyancyCenter = B;
    e.buoyancyForce = F_buoyant;
    e.waterDragForce = F_drag;
    e.dampingForce = F_linearWaterDrag;
    e.torqueDebug = torque_total;
    e.angularVelocityDebug = e.angularVelocity;


}

void PhysicsSystem::resolveBuoyantBodyCollisions(Scene* scene) {
    std::vector<Entity*> bodies;
    for (auto& e : scene->entities) {
        if (!e.visible || !e.hasCollision || !e.isBuoyant || e.isCargo) continue;
        bodies.push_back(&e);
    }

    constexpr int solverIterations = 4;
    for (int iteration = 0; iteration < solverIterations; ++iteration) {
        for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
            for (int j = i + 1; j < static_cast<int>(bodies.size()); ++j) {
                g_collisionChecks++;
                resolveCollisionBuoyantBodies(bodies[i], bodies[j]);
            }
        }
    }
}

void PhysicsSystem::resolveCollisionBuoyantBodies(Entity* a, Entity* b) {
    if (!a || !b || a == b) return;

    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    float penetration = 0.0f;
    bool collided = false;

    if (a->buoyancyType == 0 && b->buoyancyType == 0) {
        glm::vec3 diff = a->position - b->position;
        float distSq = glm::dot(diff, diff);
        float rSum = a->radius + b->radius;
        if (distSq < rSum * rSum) {
            float dist = std::sqrt(glm::max(distSq, 0.0f));
            if (dist > 0.0001f) {
                normal = diff / dist;
            } else {
                glm::vec3 relVel = a->velocity - b->velocity;
                normal = (glm::dot(relVel, relVel) > 0.000001f) ? glm::normalize(relVel) : glm::vec3(1.0f, 0.0f, 0.0f);
            }
            penetration = rSum - dist;
            collided = true;
        }
    } else if (a->buoyancyType == 0) {
        collided = sphereVsBuoyantBox(*a, *b, normal, penetration);
    } else if (b->buoyancyType == 0) {
        collided = sphereVsBuoyantBox(*b, *a, normal, penetration);
        normal = -normal;
    } else {
        collided = buoyantBoxVsBox(*a, *b, normal, penetration);
    }

    if (!collided) return;

    applyBuoyantCollisionResponse(*a, *b, normal, penetration);
    a->color = a->originalColor;
    b->color = b->originalColor;
}

void PhysicsSystem::resolveCollisionSphereAABB(Entity* sphere, Entity* box) {
    AABB bAABB = box->getGlobalBounds();

    glm::vec3 closestP = glm::max(bAABB.minExtents, glm::min(sphere->position, bAABB.maxExtents));
    glm::vec3 offset = sphere->position - closestP;
    float distSq = glm::dot(offset, offset);
    float radiusSq = sphere->radius * sphere->radius;

    if (distSq < radiusSq) {
        glm::vec3 N(0.0f, 1.0f, 0.0f);
        float penetration = sphere->radius;

        if (distSq > 0.000001f) {
            float dist = std::sqrt(distSq);
            N = offset / dist;
            penetration = sphere->radius - dist;
        } else {
            glm::vec3 center = sphere->position;
            float faceDistances[6] = {
                center.x - bAABB.minExtents.x,
                bAABB.maxExtents.x - center.x,
                center.y - bAABB.minExtents.y,
                bAABB.maxExtents.y - center.y,
                center.z - bAABB.minExtents.z,
                bAABB.maxExtents.z - center.z
            };
            glm::vec3 faceNormals[6] = {
                glm::vec3(-1.0f, 0.0f, 0.0f),
                glm::vec3( 1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(0.0f,  1.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, -1.0f),
                glm::vec3(0.0f, 0.0f,  1.0f)
            };

            int closestFace = 0;
            for (int i = 1; i < 6; ++i) {
                if (faceDistances[i] < faceDistances[closestFace]) {
                    closestFace = i;
                }
            }
            N = faceNormals[closestFace];
            penetration = sphere->radius + glm::max(faceDistances[closestFace], 0.0f);
        }

        sphere->position += N * penetration;
        
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
    float distSq = glm::dot(diff, diff);
    float rSum = s1->radius + s2->radius;
    
    if (distSq < rSum * rSum) {
        float dist = std::sqrt(glm::max(distSq, 0.0f));
        glm::vec3 N(1.0f, 0.0f, 0.0f);
        if (dist > 0.0001f) {
            N = diff / dist;
        } else {
            glm::vec3 relVel = s1->velocity - s2->velocity;
            float relVelSq = glm::dot(relVel, relVel);
            if (relVelSq > 0.000001f) {
                N = glm::normalize(relVel);
            }
        }
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
    
    constexpr int solverIterations = 4;
    for (int iteration = 0; iteration < solverIterations; ++iteration) {
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
}

void PhysicsSystem::updateGrid(Scene* scene, float deltaTime) {
    std::vector<int> dynamicSpheres;
    
    // 1. Gather dynamic entities by index.
    for (int i = 0; i < scene->entities.size(); i++) {
        auto& e = scene->entities[i];
        if (!e.hasCollision) continue;
        
        if (e.name == "DynamicSphere") {
            dynamicSpheres.push_back(i);
        }
    }
    
    // 2. Resolve using grid via index loops.
    constexpr int solverIterations = 4;
    for (int iteration = 0; iteration < solverIterations; ++iteration) {
        // Rebuild dynamic cells each solver pass because collision resolution moves spheres.
        for (int x=0; x<7; x++) {
            for (int y=0; y<7; y++) {
                for (int z=0; z<7; z++) {
                    grid[x][y][z].dynamicEntities.clear();
                }
            }
        }
        for (int sphereIdx : dynamicSpheres) {
            Entity& e = scene->entities[sphereIdx];
            glm::ivec3 cell = getGridCell(e.position);
            grid[cell.x][cell.y][cell.z].dynamicEntities.push_back(sphereIdx);
        }

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
}

