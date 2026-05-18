layout(binding = 9) uniform sampler2D rippleHeightTex;
uniform bool u_useRipple;
uniform bool u_enableWaterWaves;
uniform vec2 u_rippleOrigin;
uniform float u_rippleWorldSize;
uniform float u_rippleTexelWorldSize;
uniform float u_visualRippleScale;
uniform float u_physicsRippleScale;
uniform float u_rippleNormalStrength;

vec2 rippleWorldToUv(vec2 worldXZ) {
    return (worldXZ - u_rippleOrigin) / u_rippleWorldSize;
}

bool rippleInside(vec2 uv) {
    return all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)));
}

float sampleRippleHeight(vec2 worldXZ) {
    if (!u_useRipple) return 0.0;
    vec2 uv = rippleWorldToUv(worldXZ);
    if (!rippleInside(uv)) return 0.0;
    return texture(rippleHeightTex, uv).r;
}

float sampleRippleHeightSmoothed(vec2 worldXZ) {
    if (!u_useRipple) return 0.0;
    float stepSize = max(u_rippleTexelWorldSize, 0.001);
    float center = sampleRippleHeight(worldXZ) * 4.0;
    float axial = sampleRippleHeight(worldXZ + vec2(stepSize, 0.0)) +
                  sampleRippleHeight(worldXZ - vec2(stepSize, 0.0)) +
                  sampleRippleHeight(worldXZ + vec2(0.0, stepSize)) +
                  sampleRippleHeight(worldXZ - vec2(0.0, stepSize));
    float diagonal = sampleRippleHeight(worldXZ + vec2(stepSize, stepSize)) +
                     sampleRippleHeight(worldXZ + vec2(stepSize, -stepSize)) +
                     sampleRippleHeight(worldXZ + vec2(-stepSize, stepSize)) +
                     sampleRippleHeight(worldXZ + vec2(-stepSize, -stepSize));
    return (center + axial * 2.0 + diagonal) / 16.0;
}

vec3 sampleRippleNormal(vec2 worldXZ) {
    if (!u_useRipple) return vec3(0.0, 1.0, 0.0);
    float stepSize = max(u_rippleTexelWorldSize, 0.001);
    float hL = sampleRippleHeight(worldXZ - vec2(stepSize, 0.0));
    float hR = sampleRippleHeight(worldXZ + vec2(stepSize, 0.0));
    float hD = sampleRippleHeight(worldXZ - vec2(0.0, stepSize));
    float hU = sampleRippleHeight(worldXZ + vec2(0.0, stepSize));
    return normalize(vec3(-(hR - hL), 2.0 * stepSize, -(hU - hD)));
}

vec3 sampleRippleNormalSmoothed(vec2 worldXZ) {
    if (!u_useRipple) return vec3(0.0, 1.0, 0.0);
    float stepSize = max(u_rippleTexelWorldSize, 0.001);
    float hL = sampleRippleHeightSmoothed(worldXZ - vec2(stepSize, 0.0));
    float hR = sampleRippleHeightSmoothed(worldXZ + vec2(stepSize, 0.0));
    float hD = sampleRippleHeightSmoothed(worldXZ - vec2(0.0, stepSize));
    float hU = sampleRippleHeightSmoothed(worldXZ + vec2(0.0, stepSize));
    return normalize(vec3(-(hR - hL), 2.0 * stepSize, -(hU - hD)));
}

vec3 combineWaterNormals(vec3 oceanNormal, vec3 rippleNormal) {
    vec3 rippleSlope = vec3(rippleNormal.x, 0.0, rippleNormal.z);
    return normalize(oceanNormal + rippleSlope * (0.85 * max(u_rippleNormalStrength, 0.0)));
}

struct WaterSurface {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    float oceanHeight;
    float rippleHeight;
    float totalHeight;
};

WaterSurface queryWaterSurfaceFiltered(vec2 worldXZ, float baseWaterLevel, float waveTime, bool smoothRipples);

WaterSurface queryWaterSurface(vec2 worldXZ, float baseWaterLevel, float waveTime) {
    return queryWaterSurfaceFiltered(worldXZ, baseWaterLevel, waveTime, false);
}

WaterSurface queryWaterSurfaceFiltered(vec2 worldXZ, float baseWaterLevel, float waveTime, bool smoothRipples) {
    if (!u_enableWaterWaves) {
        float rippleHeight = smoothRipples ? sampleRippleHeightSmoothed(worldXZ) * u_physicsRippleScale : sampleRippleHeight(worldXZ) * u_visualRippleScale;
        vec3 rippleNormal = smoothRipples ? sampleRippleNormalSmoothed(worldXZ) : sampleRippleNormal(worldXZ);
        rippleNormal = normalize(mix(vec3(0.0, 1.0, 0.0), rippleNormal, smoothRipples ? min(u_physicsRippleScale, 0.45) : min(u_rippleNormalStrength, 1.6)));

        WaterSurface flatSurface;
        flatSurface.position = vec3(worldXZ.x, baseWaterLevel, worldXZ.y);
        flatSurface.position.y += rippleHeight;
        flatSurface.normal = combineWaterNormals(vec3(0.0, 1.0, 0.0), rippleNormal);
        flatSurface.tangent = vec3(1.0, 0.0, 0.0);
        flatSurface.bitangent = vec3(0.0, 0.0, 1.0);
        flatSurface.oceanHeight = baseWaterLevel;
        flatSurface.rippleHeight = rippleHeight;
        flatSurface.totalHeight = flatSurface.position.y;
        return flatSurface;
    }

    GerstnerSurface ocean = evaluateGerstnerSurface(vec3(worldXZ.x, baseWaterLevel, worldXZ.y), waveTime);
    float rippleHeight = smoothRipples ? sampleRippleHeightSmoothed(ocean.position.xz) * u_physicsRippleScale : sampleRippleHeight(ocean.position.xz) * u_visualRippleScale;
    vec3 rippleNormal = smoothRipples ? sampleRippleNormalSmoothed(ocean.position.xz) : sampleRippleNormal(ocean.position.xz);
    rippleNormal = normalize(mix(vec3(0.0, 1.0, 0.0), rippleNormal, smoothRipples ? min(u_physicsRippleScale, 0.45) : min(u_rippleNormalStrength, 1.6)));

    WaterSurface surface;
    surface.position = ocean.position;
    surface.position.y += rippleHeight;
    surface.normal = combineWaterNormals(ocean.normal, rippleNormal);
    surface.tangent = ocean.tangent;
    surface.bitangent = ocean.bitangent;
    surface.oceanHeight = ocean.position.y;
    surface.rippleHeight = rippleHeight;
    surface.totalHeight = surface.position.y;
    return surface;
}

float queryWaterHeight(vec2 worldXZ, float baseWaterLevel, float waveTime) {
    return queryWaterSurface(worldXZ, baseWaterLevel, waveTime).totalHeight;
}
