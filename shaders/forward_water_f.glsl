#version 450 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 FragPosLightSpace;
} fs_in;

// --- 固定貼圖單元綁定 ---
layout(binding = 10) uniform sampler2D albedoMap;
layout(binding = 11) uniform sampler2D normalMap;
layout(binding = 12) uniform sampler2D metallicMap;
layout(binding = 13) uniform sampler2D roughnessMap;
layout(binding = 14) uniform sampler2D aoMap;
layout(binding = 15) uniform sampler2D emissiveMap;

layout(binding = 3)  uniform sampler2D shadowMap;
layout(binding = 5)  uniform samplerCube irradianceMap;
layout(binding = 6)  uniform samplerCube prefilterMap;
layout(binding = 7)  uniform sampler2D   brdfLUT;
layout(binding = 8)  uniform samplerCubeShadow pointShadowMap;

struct Material { float ambientStrength; };
uniform Material material;

struct Light { vec3 position; vec3 color; };
uniform Light light1;
uniform Light light2;

uniform vec3 objectColor; 
uniform bool useNormalMap;
uniform bool isLightSource;
uniform bool isWater;
uniform vec3 viewPos;
uniform float roughness; 
uniform float metallic;  
uniform float reflectivity;
uniform float time;
uniform float far_plane;
uniform float u_shadowBias;
uniform float u_pcfRadius;

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
    if(length(light1.color) < 0.01) return 0.0;
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

float PointShadowCalculation(vec3 fragPos, vec3 normal) {
    vec3 fragToLight = fragPos - light2.position;
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
    if(isLightSource) {
        FragColor = vec4(objectColor, 1.0); return;
    }

    // 1. 採樣與 PBR 頻道解析
    vec3 texAlbedo = texture(albedoMap, fs_in.TexCoords).rgb;
    vec3 baseColor = pow(texAlbedo, vec3(2.2)) * objectColor;
    
    // GLTF Packing: R=Occlusion, G=Roughness, B=Metallic
    float m = texture(metallicMap, fs_in.TexCoords).b * metallic;
    float r = texture(roughnessMap, fs_in.TexCoords).g * roughness;

    float aoSample = texture(aoMap, fs_in.TexCoords).r * material.ambientStrength;
    vec3 emissive = pow(texture(emissiveMap, fs_in.TexCoords).rgb, vec3(2.2)) * 2.0; // 自發光增益
    
    r = clamp(r, 0.04, 1.0);

    // 2. 法線處理
    vec3 N_geom = normalize(fs_in.Normal);
    vec3 T = normalize(fs_in.Tangent);
    T = normalize(T - dot(T, N_geom) * N_geom);
    vec3 B = cross(N_geom, T);
    mat3 TBN = mat3(T, B, N_geom);

    vec3 N;
    if(isWater) {
        vec2 uv1 = fs_in.TexCoords * 0.8 + vec2(time * 0.01, time * 0.005);
        vec2 uv2 = fs_in.TexCoords * 2.2 + vec2(time * -0.015, time * 0.012);
        vec2 uv3 = fs_in.TexCoords * 5.0 + vec2(time * 0.02, time * -0.02);
        vec3 n1 = texture(normalMap, uv1).rgb * 2.0 - 1.0;
        vec3 n2 = texture(normalMap, uv2).rgb * 2.0 - 1.0;
        vec3 n3 = texture(normalMap, uv3).rgb * 2.0 - 1.0;
        vec3 mixedNormal = normalize(n1 + n2 * 0.6 + n3 * 0.3);
        mixedNormal.xy *= 2.5; 
        N = normalize(TBN * normalize(mixedNormal));
    } else if(useNormalMap) {
        vec3 tangentNormal = texture(normalMap, fs_in.TexCoords).rgb * 2.0 - 1.0;
        tangentNormal.xy *= 1.5;
        N = normalize(TBN * normalize(tangentNormal));
    } else {
        N = N_geom;
    }

    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 reflectN = isWater ? normalize(mix(N, N_geom, 0.3)) : N;
    vec3 R = reflect(-V, reflectN);
    float NoV = max(dot(N, V), 0.0);

    // 3. 材質與 F0
    vec3 albedo = baseColor;
    if(isWater) {
        float fresnelWater = pow(1.0 - NoV, 5.0);
        vec3 deepColor = vec3(0.0, 0.02, 0.05); 
        albedo = mix(deepColor, albedo * 0.6, fresnelWater);
    }
    
    float specIntensity = reflectivity * 8.0; 
    if(isWater) specIntensity *= 1.5;
    vec3 F0 = mix(vec3(0.04), albedo, m);

    // 4. 直接光照
    vec3 directLo = vec3(0.0);
    // Light 1 (Sun) - Directional Light
    if(length(light1.color) > 0.01) {
        vec3 L = normalize(light1.position); vec3 H = normalize(V + L);
        float shadow = ShadowCalculation(fs_in.FragPosLightSpace, N, L);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, r); 
        float G = GeometrySmith(N, V, L, r); 
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * NoV * NdotL + 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - m);
        directLo += (kD * albedo / PI + spec * specIntensity) * light1.color * NdotL * (1.0 - shadow);
    }
    // Light 2 (Point)
    if(length(light2.color) > 0.01) {
        vec3 L = normalize(light2.position - fs_in.FragPos); vec3 H = normalize(V + L);
        float dist = length(light2.position - fs_in.FragPos);
        float atten = 1.0 / (dist * dist + 0.001);
        float shadow = PointShadowCalculation(fs_in.FragPos, N);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, r); 
        float G = GeometrySmith(N, V, L, r); 
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * NoV * NdotL + 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - m);
        directLo += (kD * albedo / PI + spec * specIntensity) * light2.color * atten * NdotL * (1.0 - shadow) * 100.0;
    }

    // 5. 間接光照 (IBL)
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
    if(isWater) ambient *= 0.5;
    
    // --- 疊加自發光 ---
    vec3 color = directLo + ambient + emissive;
    
    color = color / (color + vec3(1.0)); 
    color = pow(color, vec3(1.0/2.2));   

    float alpha = 1.0;
    if (isWater) {
        float fresnelWater = pow(1.0 - NoV, 5.0);
        alpha = mix(0.4, 0.85, fresnelWater);
    } else if (!isLightSource) {
        float F_fresnel = pow(1.0 - NoV, 5.0);
        alpha = mix(0.15, 0.8, F_fresnel);
        alpha = clamp(alpha * (reflectivity + 0.1), 0.05, 0.95);
    }

    FragColor = vec4(color, alpha);
}
