#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(r32f, binding = 0) readonly uniform image2D u_prevHeight;
layout(r32f, binding = 1) readonly uniform image2D u_currHeight;
layout(r32f, binding = 2) writeonly uniform image2D u_nextHeight;

uniform int u_resolution;
uniform float u_cfl;
uniform float u_damping;
uniform float u_amplitudeClamp;

float loadHeight(ivec2 p) {
    ivec2 clamped = clamp(p, ivec2(0), ivec2(u_resolution - 1));
    return imageLoad(u_currHeight, clamped).r;
}

void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= u_resolution || p.y >= u_resolution) return;

    float h = imageLoad(u_currHeight, p).r;
    float hPrev = imageLoad(u_prevHeight, p).r;
    float hL = loadHeight(p + ivec2(-1, 0));
    float hR = loadHeight(p + ivec2(1, 0));
    float hD = loadHeight(p + ivec2(0, -1));
    float hU = loadHeight(p + ivec2(0, 1));

    float laplacian = hL + hR + hD + hU - 4.0 * h;
    float velocity = (h - hPrev) * u_damping;
    float hNext = h + velocity + u_cfl * laplacian;
    float neighborAverage = (hL + hR + hD + hU) * 0.25;
    hNext = mix(hNext, neighborAverage, 0.008);
    hNext -= h * 0.0015;

    float edgeDistance = min(min(p.x, p.y), min(u_resolution - 1 - p.x, u_resolution - 1 - p.y));
    float edgeFade = smoothstep(0.0, 10.0, edgeDistance);
    hNext *= mix(0.80, 0.996, edgeFade);
    hNext = clamp(hNext, -u_amplitudeClamp, u_amplitudeClamp);

    if (isnan(hNext) || isinf(hNext)) {
        hNext = 0.0;
    }

    imageStore(u_nextHeight, p, vec4(hNext, 0.0, 0.0, 1.0));
}
