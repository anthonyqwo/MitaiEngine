#version 450 core
out vec4 FragColor;
in vec3 localPos;
uniform samplerCube environmentMap;
uniform float roughness;

const float PI = 3.14159265359;

// 1. 低差異序列與重要性採樣 (GGX)
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N) { return vec2(float(i)/float(N), RadicalInverse_VdC(i)); }

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness*roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float DistributionGGX(float NdotH, float roughness) {
    float a = roughness*roughness; float a2 = a*a;
    float NdotH2 = NdotH*NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0); denom = PI * denom * denom;
    return nom / denom;
}

void main() {
    vec3 N = normalize(localPos);
    vec3 R = N; vec3 V = R;
    const uint SAMPLE_COUNT = 2048u; // 增加採樣數
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;
    float envSize = float(textureSize(environmentMap, 0).x);

    for(uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0) {
            // 筆記核心：根據 PDF 計算 Lod 以消除雜訊
            float D = DistributionGGX(max(dot(N, H), 0.0), roughness);
            float pdf = (D * max(dot(N, H), 0.0) / (4.0 * max(dot(V, H), 0.0))) + 0.0001;
            float saTexel  = 4.0 * PI / (6.0 * envSize * envSize);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            float lod = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 

            prefilteredColor += textureLod(environmentMap, L, lod).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    FragColor = vec4(prefilteredColor / totalWeight, 1.0);
}
