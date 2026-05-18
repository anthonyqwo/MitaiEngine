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
layout(binding = 8) uniform samplerCubeShadow pointShadowMap;
layout(binding = 9) uniform sampler2D gEmissiveMap;

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
uniform float u_shadowBias;
uniform float u_pcfRadius;
uniform int u_visualisationMode; // 0=None, 1=Albedo, 2=Normals, 3=Roughness, 4=Metallic, 5=Depth

const float PI = 3.14159265359;

const vec2 poissonDisk[9] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38208752, 0.27676845),
    vec2(0.20340809, -0.38208752),
    vec2(0.74201624, 0.53906216)
);

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    
    // Slope-scale depth bias scaling with dot(N, L)
    float bias = max(u_shadowBias * (1.0 - max(dot(normal, lightDir), 0.0)), u_shadowBias * 0.1);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    // 3x3 Poisson-disk PCF filtering
    for(int i = 0; i < 9; ++i) {
        float pcfDepth = texture(shadowMap, projCoords.xy + poissonDisk[i] * texelSize * u_pcfRadius).r;
        shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
    }
    return shadow / 9.0;
}

float PointShadowCalculation(vec3 fragPos, vec3 lightPos) {
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    float bias = u_shadowBias * 10.0;
    
    // Single-tap PCF using samplerCubeShadow
    float refDepth = (currentDepth - bias) / far_plane;
    float shadowSample = texture(pointShadowMap, vec4(fragToLight, refDepth));
    return 1.0 - shadowSample;
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
    vec2 tc = gl_FragCoord.xy / vec2(textureSize(gPosition, 0));
    vec3 FragPos = texture(gPosition, tc).rgb;
    vec4 NormalRefl = texture(gNormal, tc);
    vec3 N = NormalRefl.rgb;
    
    // 如果法線長度小於0.5，代表這是天空盒（或是背景），直接過
    if (length(N) < 0.5) {
        discard;
    }

    float reflectivity = NormalRefl.a;
    vec3 albedo = texture(gAlbedo, tc).rgb;
    vec4 pbr = texture(gPBR, tc);
    float m = pbr.r;
    float r = pbr.g;
    float aoSample = pbr.b;
    
    if (u_visualisationMode > 0) {
        if (u_visualisationMode == 1) {
            FragColor = vec4(albedo, 1.0);
        } else if (u_visualisationMode == 2) {
            FragColor = vec4(N * 0.5 + 0.5, 1.0);
        } else if (u_visualisationMode == 3) {
            FragColor = vec4(vec3(r), 1.0);
        } else if (u_visualisationMode == 4) {
            FragColor = vec4(vec3(m), 1.0);
        } else if (u_visualisationMode == 5) {
            float d = length(FragPos - viewPos) * 0.05;
            FragColor = vec4(vec3(d), 1.0);
        }
        return;
    }
    
    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 0.0);
    
    float specIntensity = reflectivity * 8.0; 
    vec3 F0 = mix(vec3(0.04), albedo, m);
    vec3 directLo = vec3(0.0);

    // 1. 主燈 (Main Sun) - Directional Light, no distance attenuation
    if(length(dirLight.color) > 0.01) {
        vec3 L = normalize(dirLight.position);  // position used as light direction
        vec3 H = normalize(V + L);
        vec4 fragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
        float shadow = ShadowCalculation(fragPosLightSpace, N, L);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, r); 
        float G = GeometrySmith(N, V, L, r); 
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * NoV * NdotL + 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - m);
        directLo += (kD * albedo / PI + spec * specIntensity) * dirLight.color * NdotL * (1.0 - shadow);
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
    vec3 emissive = texture(gEmissiveMap, tc).rgb;
    
    vec3 color = directLo + ambient + emissive; 
    
    color = color / (color + vec3(1.0)); 
    color = pow(color, vec3(1.0/2.2));   
    FragColor = vec4(color, 1.0);
}
