const int GERSTNER_WAVE_COUNT = 4;
const float GERSTNER_PI = 3.14159265359;
const float GERSTNER_EPSILON = 0.0001;
const float GERSTNER_MAX_STEEPNESS = 0.95;

struct GerstnerWave {
    vec2 direction;
    float amplitude;
    float wavelength;
    float speed;
    float steepness;
};

struct GerstnerSurface {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
};

uniform GerstnerWave waves[GERSTNER_WAVE_COUNT];

vec2 safeWaveDirection(vec2 direction) {
    float lenSq = dot(direction, direction);
    if (lenSq < GERSTNER_EPSILON) {
        return vec2(1.0, 0.0);
    }
    return direction * inversesqrt(lenSq);
}

GerstnerSurface evaluateGerstnerSurface(vec3 baseWorldPos, float waveTime) {
    vec2 xz = baseWorldPos.xz;
    vec3 displacement = vec3(0.0);

    vec3 tx = vec3(1.0, 0.0, 0.0);
    vec3 tz = vec3(0.0, 0.0, 1.0);

    for (int i = 0; i < GERSTNER_WAVE_COUNT; i++) {
        vec2 d = safeWaveDirection(waves[i].direction);
        float A = max(waves[i].amplitude, 0.0);
        float L = max(waves[i].wavelength, GERSTNER_EPSILON);
        float k = 2.0 * GERSTNER_PI / L;
        float c = waves[i].speed;
        float steepness = clamp(waves[i].steepness, 0.0, GERSTNER_MAX_STEEPNESS);
        float q = steepness / max(A * k * float(GERSTNER_WAVE_COUNT), GERSTNER_EPSILON);

        float theta = k * dot(d, xz) - c * k * waveTime;
        float s = sin(theta);
        float co = cos(theta);

        displacement.x += q * A * d.x * co;
        displacement.y += A * s;
        displacement.z += q * A * d.y * co;

        tx.x -= q * A * k * d.x * d.x * s;
        tx.y += A * k * d.x * co;
        tx.z -= q * A * k * d.x * d.y * s;

        tz.x -= q * A * k * d.x * d.y * s;
        tz.y += A * k * d.y * co;
        tz.z -= q * A * k * d.y * d.y * s;
    }

    GerstnerSurface surface;
    surface.position = baseWorldPos + displacement;
    surface.tangent = normalize(tx);
    surface.bitangent = normalize(tz);
    surface.normal = normalize(cross(surface.tangent, surface.bitangent));

    if (surface.normal.y < 0.0) {
        surface.normal = -surface.normal;
    }

    return surface;
}
