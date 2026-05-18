#version 430 core
layout(local_size_x = 256) in;

struct GerstnerWave {
    vec2 direction;
    float amplitude;
    float wavelength;
    float speed;
    float steepness;
};

struct QueryPoint {
    float x;
    float z;
    float height;
    float padding;
};

layout(std430, binding = 4) buffer QueryBuffer {
    QueryPoint queries[];
};

uniform int numQueries;
uniform float time;
uniform GerstnerWave waves[4];

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= numQueries) return;
    
    float x = queries[idx].x;
    float z = queries[idx].z;
    
    float dy = 0.0;
    
    // Evaluate sum of 4 Gerstner waves
    for (int i = 0; i < 4; i++) {
        vec2 d = normalize(waves[i].direction);
        float A = waves[i].amplitude;
        float L = waves[i].wavelength;
        float k = 2.0 * 3.14159265359 / L;
        float c = waves[i].speed;
        
        float theta = k * dot(d, vec2(x, z)) - c * k * time;
        dy += A * sin(theta);
    }
    
    queries[idx].height = 4.0 + dy; // base water level is 4.0
}
