#version 430 core
layout(local_size_x = 256) in;

#include "gerstner_common.glsl"
#include "ripple_common.glsl"

struct WaterQuery {
    vec4 samplePosition;
    vec4 waterPosition;
    vec4 waterNormal;
    vec4 waterData;
};

layout(std430, binding = 4) buffer QueryBuffer {
    WaterQuery queries[];
};

uniform int numQueries;
uniform float time;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= numQueries) return;

    WaterSurface surface = queryWaterSurfaceFiltered(queries[idx].samplePosition.xz, 4.0, time, true);

    queries[idx].waterPosition = vec4(surface.position, 0.0);
    queries[idx].waterNormal = vec4(surface.normal, 0.0);
    queries[idx].waterData = vec4(surface.oceanHeight, surface.rippleHeight, surface.totalHeight, 0.0);
}
