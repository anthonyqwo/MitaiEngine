#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(r32f, binding = 0) coherent uniform image2D u_currHeight;
layout(r32f, binding = 1) coherent uniform image2D u_prevHeight;

struct RippleImpulse {
    vec4 positionRadius;
    vec4 strength;
};

layout(std430, binding = 5) readonly buffer RippleImpulseBuffer {
    RippleImpulse impulses[];
};

uniform int u_impulseCount;
uniform int u_resolution;
uniform vec2 u_origin;
uniform float u_worldSize;
uniform float u_maxRippleHeight;

void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= u_resolution || p.y >= u_resolution) return;

    vec2 uv = (vec2(p) + vec2(0.5)) / float(u_resolution);
    vec2 worldXZ = u_origin + uv * u_worldSize;

    float impulseVelocity = 0.0;
    for (int i = 0; i < u_impulseCount; ++i) {
        vec3 pos = impulses[i].positionRadius.xyz;
        float radius = max(impulses[i].positionRadius.w, 0.001);
        float strength = impulses[i].strength.x;
        float dist = length(worldXZ - pos.xz);
        float x = dist / radius;
        if (x > 1.60) continue;

        float center = exp(-x * x * 7.5);
        float ring = exp(-(x - 0.92) * (x - 0.92) * 14.0);
        float outerFade = 1.0 - smoothstep(1.15, 1.60, x);
        float ringWave = (center - ring * 0.055) * outerFade;
        impulseVelocity += strength * ringWave;
    }

    float curr = imageLoad(u_currHeight, p).r;
    float prev = imageLoad(u_prevHeight, p).r;
    float limit = clamp(u_maxRippleHeight, 0.025, 0.08);
    if (abs(impulseVelocity) < 0.00035) {
        impulseVelocity = 0.0;
    }
    float nextCurr = clamp(curr + impulseVelocity * 0.35, -limit, limit);
    float nextPrev = clamp(prev + impulseVelocity * 0.28, -limit, limit);

    imageStore(u_currHeight, p, vec4(nextCurr, 0.0, 0.0, 1.0));
    imageStore(u_prevHeight, p, vec4(nextPrev, 0.0, 0.0, 1.0));
}
