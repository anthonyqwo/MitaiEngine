#define GLM_ENABLE_EXPERIMENTAL
#include "PhysicsSystem.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "ResourceManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
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

struct QueryPoint {
    float x;
    float z;
    float height;
    float padding;
};

void PhysicsSystem::update(Scene* scene, float deltaTime, bool useGrid) {
    g_collisionChecks = 0;
    
    // Gather coordinates for all buoyant queries on the GPU
    std::vector<QueryPoint> queryPoints;
    for (auto& e : scene->entities) {
        if (!e.isBuoyant || !e.visible) continue;
        
        if (e.buoyancyType == 0) { // Sphere
            QueryPoint q;
            q.x = e.position.x;
            q.z = e.position.z;
            q.height = 4.0f;
            q.padding = 0.0f;
            queryPoints.push_back(q);
        } else { // Slab or Open-top Box
            float W = e.scale.x;
            float H = e.scale.y;
            float D = e.scale.z;
            
            // Flooding corners for Open-top Box (type 2)
            if (e.buoyancyType == 2 && e.floodLevel < 1.0f) {
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
                    QueryPoint q;
                    q.x = worldC.x;
                    q.z = worldC.z;
                    q.height = 4.0f;
                    q.padding = 0.0f;
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
                        QueryPoint q;
                        q.x = worldPos.x;
                        q.z = worldPos.z;
                        q.height = 4.0f;
                        q.padding = 0.0f;
                        queryPoints.push_back(q);
                    }
                }
            }
        }
    }

    std::vector<float> heights;
    if (!queryPoints.empty()) {
        static unsigned int querySSBO = 0;
        if (querySSBO == 0) {
            glGenBuffers(1, &querySSBO);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, querySSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, queryPoints.size() * sizeof(QueryPoint), queryPoints.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, querySSBO);
        
        Shader* waveShader = ResourceManager::getShader("waveQuery");
        if (waveShader) {
            waveShader->use();
            waveShader->setInt("numQueries", (int)queryPoints.size());
            waveShader->setFloat("time", (float)glfwGetTime());
            
            // Pass 4 Gerstner Wave parameter uniforms
            struct CPUWave {
                glm::vec2 direction;
                float amplitude;
                float wavelength;
                float speed;
                float steepness;
            };
            const CPUWave waves[4] = {
                { glm::vec2(1.0f, 0.2f),   0.15f, 6.0f, 1.8f, 0.4f },
                { glm::vec2(-0.7f, 0.7f),  0.10f, 3.5f, 2.5f, 0.3f },
                { glm::vec2(0.1f, 1.0f),   0.08f, 2.0f, 1.2f, 0.2f },
                { glm::vec2(-0.3f, -0.9f), 0.04f, 1.2f, 0.8f, 0.1f }
            };
            for (int i = 0; i < 4; i++) {
                std::string prefix = "waves[" + std::to_string(i) + "].";
                waveShader->setVec2(prefix + "direction", waves[i].direction);
                waveShader->setFloat(prefix + "amplitude", waves[i].amplitude);
                waveShader->setFloat(prefix + "wavelength", waves[i].wavelength);
                waveShader->setFloat(prefix + "speed", waves[i].speed);
                waveShader->setFloat(prefix + "steepness", waves[i].steepness);
            }
            
            int numGroups = ((int)queryPoints.size() + 255) / 256;
            glDispatchCompute(numGroups, 1, 1);
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            
            // Read back data
            std::vector<QueryPoint> resultPoints(queryPoints.size());
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, resultPoints.size() * sizeof(QueryPoint), resultPoints.data());
            
            heights.reserve(resultPoints.size());
            for (const auto& q : resultPoints) {
                heights.push_back(q.height);
            }
        } else {
            heights.assign(queryPoints.size(), 4.0f);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    // Euler integration for velocities
    int heightIndex = 0;

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
        vLocal.x *= (1.0f - 4.0f * deltaTime);
        vLocal.y *= (1.0f - 4.0f * deltaTime);
        vLocal.z *= (1.0f - 4.0f * deltaTime);
        
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
        } else if (gravityLocal.y > 0.0f && boxCargo->cargoLocalOffset.y > -0.10f) {
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
                updateBuoyancy(e, deltaTime, heights, heightIndex, cargoMass, cargoOffset);
            } else {
                updateBuoyancy(e, deltaTime, heights, heightIndex);
            }
        } else if (e.name == "DynamicSphere") {
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

void PhysicsSystem::updateBuoyancy(Entity& e, float deltaTime, const std::vector<float>& heights, int& heightIndex, float cargoMass, glm::vec3 cargoOffset) {
    // 限制物理更新的最大步長以防崩潰 (Cap deltaTime to prevent explosion)
    if (deltaTime > 0.05f) deltaTime = 0.05f;
    if (deltaTime <= 0.0f) return;

    float yw = 4.0f; // Default water plane fallback
    float rho = 1000.0f; // ρ_water = 1000 kg/m³
    float g = 9.81f; // g = 9.81 m/s²
    
    // Combined system mass
    float M = e.mass + cargoMass;
    
    // Shift of Center of Mass (CoM) in local space
    glm::vec3 CoM_local = (M > 0.001f) ? (cargoMass / M) * cargoOffset : glm::vec3(0.0f);
    glm::vec3 worldCoM = e.position + e.orientation * CoM_local;

    // 1. 計算局部轉動慣量 (Local Moment of Inertia) 與 體積 (Total Volume)
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
    }

    // 2. 計算排水體積 (V_sub) 與 浮力中心 (B)
    float V_sub = 0.0f;
    glm::vec3 B(0.0f);

    if (e.buoyancyType == 0) { // Hollow Sphere
        float R = e.radius;
        float local_yw = yw;
        if (!heights.empty() && heightIndex < heights.size()) {
            local_yw = heights[heightIndex++];
        }
        float h = local_yw - (e.position.y - R); // Submerged depth from bottom
        if (h <= 0.0f) {
            V_sub = 0.0f;
            B = e.position;
        } else if (h >= 2.0f * R) {
            V_sub = V_total;
            B = e.position;
        } else {
            V_sub = (glm::pi<float>() * h * h / 3.0f) * (3.0f * R - h);
            float y_max_local = local_yw - e.position.y;
            float integral = glm::pi<float>() * ( (R * R * y_max_local * y_max_local) / 2.0f - (y_max_local * y_max_local * y_max_local * y_max_local) / 4.0f - (R * R * R * R) / 4.0f );
            float y_local_buoyancy = integral / V_sub;
            B = e.position + e.orientation * glm::vec3(0.0f, y_local_buoyancy, 0.0f);
        }
        e.submergedFraction = V_sub / V_total;
    } else { // Wooden Slab or Open-top Box
        // Continuous Flooding & Drainage logic with Hysteresis
        bool allCornersAbove = true;
        int numSubmergedCorners = 0;

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

            if (e.floodLevel < 1.0f) {
                // Determine precise submerged corners from heights buffer
                for (int i = 0; i < 4; ++i) {
                    glm::vec3 worldC = e.position + e.orientation * corners[i];
                    float corner_yw = yw;
                    if (!heights.empty() && heightIndex < heights.size()) {
                        corner_yw = heights[heightIndex++];
                    }
                    if (worldC.y < corner_yw) {
                        numSubmergedCorners++;
                    }
                }
                allCornersAbove = (numSubmergedCorners == 0);
            } else {
                // Using yw approximation when fully flooded (GPU queries skipped)
                for (int i = 0; i < 4; ++i) {
                    glm::vec3 worldC = e.position + e.orientation * corners[i];
                    if (worldC.y < yw) {
                        allCornersAbove = false;
                        break;
                    }
                }
            }

            // Hysteresis & Accumulation / Drainage update
            if (allCornersAbove) {
                e.dryTimer += deltaTime;
                if (e.dryTimer >= 1.5f) {
                    e.floodLevel -= 0.3f * deltaTime; // drainRate = 0.3f (fully drains in ~3.3s)
                }
            } else {
                e.dryTimer = 0.0f; // Reset timer when wet
                if (numSubmergedCorners >= 2) {
                    e.floodLevel += 0.4f * deltaTime; // ingressRate = 0.4f (fully floods in 2.5s)
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
                            weight = 1.0f - e.floodLevel; // Smoothly interpolate buoyancy!
                        }
                    }

                    glm::vec3 worldPos = e.position + e.orientation * glm::vec3(lx, ly, lz);
                    float point_yw = yw;
                    if (!heights.empty() && heightIndex < heights.size()) {
                        point_yw = heights[heightIndex++];
                    }
                    if (worldPos.y < point_yw) {
                        float effectiveV = dV * weight;
                        V_sub += effectiveV;
                        B_sum += worldPos * effectiveV;
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

    // 3. 計算浮力、重力、阻尼 (Gravity, Buoyant, Damping Forces & Torques)
    glm::vec3 F_gravity(0.0f, -M * g, 0.0f);
    glm::vec3 F_buoyant(0.0f, rho * g * V_sub, 0.0f);
    
    // 阻尼力 (Linear Drag): 包含水阻力(正比於淹沒比例)與極小空氣阻力
    float Cd = 4.0f; // 水阻力係數
    glm::vec3 F_drag = - (Cd * e.submergedFraction + 0.1f) * M * e.velocity;

    glm::vec3 F_total = F_gravity + F_buoyant + F_drag;

    // 扭矩計算 (Buoyancy Torque: Center of Buoyancy vs. Center of Mass)
    glm::vec3 torque_buoyancy = glm::cross(B - worldCoM, F_buoyant);

    // Apply interactive soft spring-damper joint constraint if grabbed!
    if (e.isGrabbed) {
        glm::vec3 worldGrabPos = e.position + e.orientation * e.localGrabOffset;
        glm::vec3 vGrab = e.velocity + glm::cross(e.angularVelocity, worldGrabPos - worldCoM);
        
        float Ks = 15000.0f;
        float Kd = 250.0f;
        
        glm::vec3 F_spring = Ks * (e.targetGrabWorld - worldGrabPos) - Kd * vGrab;
        
        // Safety Force Clamp (max 30000 N)
        float maxForce = 30000.0f;
        float fLen = glm::length(F_spring);
        if (fLen > maxForce) {
            F_spring = (F_spring / fLen) * maxForce;
        }
        
        F_total += F_spring;
        torque_buoyancy += glm::cross(worldGrabPos - worldCoM, F_spring);
    }

    // 旋轉阻尼 (Angular Drag): 正比於淹沒比例，在局部空間乘以慣性後轉回世界空間
    float Dd = 4.0f; // 轉動水阻力係數
    glm::mat3 R = glm::mat3_cast(e.orientation);
    glm::vec3 omega_local = glm::transpose(R) * e.angularVelocity;
    glm::vec3 L_local = I_local * omega_local;
    glm::vec3 L_world = R * L_local;
    glm::vec3 torque_drag = - (Dd * e.submergedFraction + 0.2f) * L_world;

    glm::vec3 torque_total = torque_buoyancy + torque_drag;

    // 4. 碰撞處理：彈力與阻尼懲罰法 (Spring-Damper Penalty Collisions with Tank Walls & Bottom)
    float ks = 20000.0f; // 彈力係數 (Spring constant)
    float kd = 300.0f;  // 阻尼係數 (Damping constant)

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
                    F_total += Fp;
                    torque_total += glm::cross(worldC - worldCoM, Fp);
                }
            }
        }
    }

    // 5. 歐拉積分 (Euler Integration) 更新狀態
    glm::vec3 accel = F_total / M;
    e.velocity += accel * deltaTime;
    worldCoM += e.velocity * deltaTime;

    glm::vec3 torque_local = glm::transpose(R) * torque_total;
    glm::vec3 alpha_local = torque_local / I_local;
    glm::vec3 alpha_world = R * alpha_local;
    e.angularVelocity += alpha_world * deltaTime;

    // Safety Angular Speed Clamp
    float angSpeed = glm::length(e.angularVelocity);
    float maxAngSpeed = 15.0f;
    if (angSpeed > maxAngSpeed) {
        e.angularVelocity = (e.angularVelocity / angSpeed) * maxAngSpeed;
    }

    glm::quat w_quat(0.0f, e.angularVelocity.x, e.angularVelocity.y, e.angularVelocity.z);
    e.orientation += (0.5f * w_quat * e.orientation) * deltaTime;
    e.orientation = glm::normalize(e.orientation);

    // Update entity geometric position from updated worldCoM
    e.position = worldCoM - e.orientation * CoM_local;

    // 6. 回寫物理計算狀態
    e.buoyancyCenter = B;
    e.buoyancyForce = F_buoyant;


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
