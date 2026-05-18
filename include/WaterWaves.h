#ifndef WATER_WAVES_H
#define WATER_WAVES_H

#include "Shader.h"
#include <array>
#include <string>
#include <glm/glm.hpp>

struct WaterWave {
    glm::vec2 direction;
    float amplitude;
    float wavelength;
    float speed;
    float steepness;
};

inline const std::array<WaterWave, 4>& getWaterWaves() {
    static const std::array<WaterWave, 4> waves = {{
        { glm::vec2(1.0f, 0.2f),   0.15f, 6.0f, 1.8f, 0.4f },
        { glm::vec2(-0.7f, 0.7f),  0.10f, 3.5f, 2.5f, 0.3f },
        { glm::vec2(0.1f, 1.0f),   0.08f, 2.0f, 1.2f, 0.2f },
        { glm::vec2(-0.3f, -0.9f), 0.04f, 1.2f, 0.8f, 0.1f }
    }};
    return waves;
}

inline void setWaterWaveUniforms(const Shader& shader) {
    const auto& waves = getWaterWaves();
    for (int i = 0; i < static_cast<int>(waves.size()); i++) {
        std::string prefix = "waves[" + std::to_string(i) + "].";
        shader.setVec2(prefix + "direction", waves[i].direction);
        shader.setFloat(prefix + "amplitude", waves[i].amplitude);
        shader.setFloat(prefix + "wavelength", waves[i].wavelength);
        shader.setFloat(prefix + "speed", waves[i].speed);
        shader.setFloat(prefix + "steepness", waves[i].steepness);
    }
}

#endif
