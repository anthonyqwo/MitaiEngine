#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gAlbedo;
layout(binding = 3) uniform sampler2D gPBR;

layout(binding = 4) uniform sampler2D shadowMap;
layout(binding = 5) uniform samplerCube irradianceMap;
layout(binding = 6) uniform samplerCube prefilterMap;
layout(binding = 7) uniform sampler2D brdfLUT;
layout(binding = 8) uniform samplerCube pointShadowMap;

struct PointLight {
    vec3 position;
    vec3 color;
    bool castShadow;
};

#define MAX_POINT_LIGHTS 32
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;

struct DirLight {
    vec3 position;
    vec3 color;
};
uniform DirLight dirLight;

uniform vec3 viewPos;
uniform mat4 lightSpaceMatrix;
uniform float far_plane;

const float PI = 3.14159265359;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.02 * (1.0 - dot(normal, lightDir)), 0.002);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    return shadow / 9.0;
}

float PointShadowCalculation(vec3 fragPos, vec3 lightPos) {
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    float shadow = 0.0; float bias = 0.15; int samples = 20;
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;
    vec3 sampleOffsetDirections[20] = vec3[] (
       vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
       vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
       vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
       vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
       vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );
    for(int i = 0; i < samples; ++i) {
        float closestDepth = texture(pointShadowMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= far_plane;
        if(currentDepth - bias > closestDepth) shadow += 1.0;
    }
    return shadow / float(samples);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / max(denom, 1e-7);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / max(denom, 1e-7);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec4 NormalRefl = texture(gNormal, TexCoords);
    vec3 N = NormalRefl.rgb;
    
    // 如果法線長度小於0.5，代表這是天空盒（或是背景），直接過
    if (length(N) < 0.5) {
        discard;
    }

    float reflectivity = NormalRefl.a;
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;
    vec4 pbr = texture(gPBR, TexCoords);
    float m = pbr.r;
    float r = pbr.g;
    float aoSample = pbr.b;
    
    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 0.0);
    
    float specIntensity = reflectivity * 8.0; 
    vec3 F0 = mix(vec3(0.04), albedo, m);
    vec3 directLo = vec3(0.0);

    // 1. 主燈 (Main Sun)
    if(length(dirLight.color) > 0.01) {
        vec3 L = normalize(dirLight.position - FragPos); 
        vec3 H = normalize(V + L);
        float dist = length(dirLight.position - FragPos);
        float atten = 1.0 / (dist * dist + 0.001); 
        vec4 fragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
        float shadow = ShadowCalculation(fragPosLightSpace, N, L);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, r); 
        float G = GeometrySmith(N, V, L, r); 
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * NoV * NdotL + 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - m);
        directLo += (kD * albedo / PI + spec * specIntensity) * dirLight.color * atten * NdotL * (1.0 - shadow) * 100.0;
    }
    
    // 2. 點光源迴圈 (Point Lights)
    for(int i = 0; i < numPointLights; i++) {
        vec3 L = normalize(pointLights[i].position - FragPos); 
        vec3 H = normalize(V + L);
        float dist = length(pointLights[i].position - FragPos);
        float atten = 1.0 / (dist * dist + 0.001);
        
        float shadow = 0.0;
        if(pointLights[i].castShadow) {
            shadow = PointShadowCalculation(FragPos, pointLights[i].position);
        }
        
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, r); 
        float G = GeometrySmith(N, V, L, r); 
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * NoV * NdotL + 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - m);
        directLo += (kD * albedo / PI + spec * specIntensity) * pointLights[i].color * atten * NdotL * (1.0 - shadow) * 100.0;
    }
    
    // 3. IBL 環境光
    vec3 F_ibl = fresnelSchlickRoughness(NoV, F0, r);
    vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - m);
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;
    const float MAX_LOD = 4.0;
    float lod = r * MAX_LOD; 
    vec3 pref = textureLod(prefilterMap, R, lod).rgb;    
    vec2 brdf = texture(brdfLUT, vec2(NoV, r)).rg;
    vec3 specularIBL = pref * (F_ibl * brdf.x + brdf.y);
    
    vec3 ambient = (kD_ibl * diffuseIBL + specularIBL * specIntensity) * aoSample;
    
    vec3 color = directLo + ambient; // Emissive is not here, drawn natively in unlit forward shader.
    
    color = color / (color + vec3(1.0)); 
    color = pow(color, vec3(1.0/2.2));   
    FragColor = vec4(color, 1.0);
}
