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

layout(binding = 0)  uniform sampler2D scenePositionMap;
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
uniform bool u_debugRippleHeatmap;
uniform vec2 u_screenSize;
uniform int u_waterDebugMode;
uniform float u_reflectionStrength;
uniform float u_fresnelStrength;
uniform float u_waveSteepness;
uniform float u_waterNormalStrength;

#include "gerstner_common.glsl"
#include "ripple_common.glsl"

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

vec3 sampleScrollingWaterNormal(vec2 uv) {
    float flowPulse = sin(time * 0.37) * 0.035;
    vec2 uv1 = uv * 0.48 + vec2(time * 0.045, time * 0.018 + flowPulse);
    vec2 uv2 = uv * 1.18 + vec2(time * -0.072, time * 0.052 - flowPulse);

    vec3 n1 = texture(normalMap, uv1).rgb * 2.0 - 1.0;
    vec3 n2 = texture(normalMap, uv2).rgb * 2.0 - 1.0;

    vec2 slope1 = n1.xy / max(n1.z, 0.18);
    vec2 slope2 = n2.xy / max(n2.z, 0.18);
    vec2 slope = (slope1 + slope2 * 0.38) * (1.42 * max(u_waterNormalStrength, 0.0) * max(u_waveSteepness, 0.1));

    return normalize(vec3(slope, 1.0));
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
        vec3 mixedNormal = sampleScrollingWaterNormal(fs_in.TexCoords);
        vec3 rippleNormal = sampleRippleNormal(fs_in.FragPos.xz);
        vec3 rippleCombined = combineWaterNormals(N_geom, rippleNormal);
        N = normalize(mix(rippleCombined, normalize(TBN * mixedNormal), 0.78));
    } else if(useNormalMap) {
        vec3 tangentNormal = texture(normalMap, fs_in.TexCoords).rgb * 2.0 - 1.0;
        tangentNormal.xy *= 1.5;
        N = normalize(TBN * normalize(tangentNormal));
    } else {
        N = N_geom;
    }

    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 reflectN = isWater ? normalize(mix(N, N_geom, 0.08)) : N;
    vec3 R = reflect(-V, reflectN);
    float NoV = max(dot(N, V), 0.0);
    float waterFresnel = isWater ? pow(clamp(1.0 - max(dot(V, N), 0.0), 0.0, 1.0), 5.0) * u_fresnelStrength : 0.0;
    waterFresnel = clamp(waterFresnel, 0.0, 1.0);
    float waveCrest = 0.0;
    float waveSlope = 0.0;
    if(isWater) {
        float geomSlope = clamp(1.0 - abs(N_geom.y), 0.0, 1.0);
        float detailSlope = clamp(1.0 - abs(N.y), 0.0, 1.0);
        waveSlope = max(geomSlope, detailSlope * 0.55);
        float rippleAbs = abs(sampleRippleHeight(fs_in.FragPos.xz));
        waveCrest = clamp(rippleAbs * 8.5 + waveSlope * 1.9 + 0.10, 0.0, 1.0);
    }

    // 3. 材質與 F0
    vec3 albedo = baseColor;
    if(isWater) {
        vec3 shallowWater = vec3(0.08, 0.42, 0.48);
        vec3 deepWater = vec3(0.02, 0.18, 0.26);
        albedo = mix(shallowWater, deepWater, 0.22 + waveSlope * 0.18);
        albedo = mix(albedo, vec3(0.62, 0.92, 0.96), waveCrest * 0.26);
    }
    
    float specIntensity = reflectivity * 8.0; 
    if(isWater) specIntensity *= mix(3.4, 10.5, waterFresnel) * (1.0 + waveCrest * 1.45 + waveSlope * 0.55) * u_reflectionStrength;
    vec3 F0 = isWater ? vec3(0.02) : mix(vec3(0.04), albedo, m);

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
    if(isWater) ambient *= 0.78;

    float waterDepth = 0.0;
    float waterAbsorption = 0.0;
    float waterRefractionStrength = 0.0;
    vec3 waterTransmission = vec3(1.0);
    float waterFog = 0.0;
    float waterDepthBlend = 0.0;
    if(isWater) {
        vec2 screenUv = clamp(gl_FragCoord.xy / max(u_screenSize, vec2(1.0)), vec2(0.0), vec2(1.0));
        vec3 sceneWorldPos = texture(scenePositionMap, screenUv).rgb;
        bool hasSceneDepth = dot(sceneWorldPos, sceneWorldPos) > 0.000001;
        float surfaceToScene = length(sceneWorldPos - fs_in.FragPos);
        waterDepth = hasSceneDepth ? clamp(surfaceToScene, 0.0, 24.0) : 4.0;
        vec3 absorptionCoefficient = vec3(0.20, 0.095, 0.045);
        waterTransmission = exp(-absorptionCoefficient * waterDepth);
        float luminanceTransmission = dot(waterTransmission, vec3(0.2126, 0.7152, 0.0722));
        waterAbsorption = clamp(1.0 - luminanceTransmission, 0.0, 1.0);
        waterFog = clamp(1.0 - exp(-waterDepth * 0.085), 0.0, 1.0);
        waterDepthBlend = smoothstep(0.15, 18.0, waterDepth);
        waterRefractionStrength = (1.0 - waterFresnel) * luminanceTransmission;
    }
    
    // --- 疊加自發光 ---
    vec3 color = directLo + ambient + emissive;
    if (isWater && u_debugRippleHeatmap) {
        float rippleH = sampleRippleHeight(fs_in.FragPos.xz);
        vec3 heatColor = rippleH >= 0.0 ? vec3(0.05, 0.38, 1.0) : vec3(1.0, 0.08, 0.03);
        float heatAmount = clamp(abs(rippleH) * 18.0, 0.0, 0.85);
        color = mix(color, heatColor, heatAmount);
    }
    
    color = color / (color + vec3(1.0)); 
    color = pow(color, vec3(1.0/2.2));   

    float alpha = 1.0;
    if (isWater) {
        vec3 shallowTint = vec3(0.20, 0.62, 0.66);
        vec3 midTint = vec3(0.10, 0.42, 0.50);
        vec3 deepTint = vec3(0.025, 0.18, 0.27);
        vec3 scatterTint = mix(shallowTint, midTint, smoothstep(0.0, 0.55, waterDepthBlend));
        scatterTint = mix(scatterTint, deepTint, smoothstep(0.42, 1.0, waterDepthBlend) * 0.62);
        scatterTint = mix(scatterTint, vec3(0.72, 0.95, 0.98), waveCrest * 0.24);

        vec3 skyReflection = textureLod(prefilterMap, R, mix(0.0, 1.25, r)).rgb;
        skyReflection = pow(max(skyReflection, vec3(0.0)), vec3(1.0 / 2.2));
        vec3 crestHighlight = vec3(0.75, 0.92, 1.0) * (waveCrest * (0.24 + waterFresnel * 0.46) * u_reflectionStrength);
        vec3 reflectionTint = skyReflection + crestHighlight;
        vec3 attenuatedTransmission = scatterTint * mix(vec3(1.0), waterTransmission, 0.32);
        vec3 scatteredVolume = mix(attenuatedTransmission, scatterTint, waterFog * 0.42);
        color = mix(scatteredVolume, color, 0.22);
        float reflectionAmount = clamp((0.20 + waterFresnel * 0.78 + waveCrest * 0.18 + waveSlope * 0.08) * u_reflectionStrength, 0.0, 0.92);
        color = mix(color, reflectionTint, reflectionAmount);
        color *= mix(1.08, 0.82, waterAbsorption);

        float baseAlpha = 0.62;
        float depthAlpha = smoothstep(0.2, 10.0, waterDepth) * 0.26;
        alpha = mix(baseAlpha + depthAlpha, 0.94, waterFresnel);
        alpha += waterFog * 0.12 + waterAbsorption * 0.10 + waveCrest * 0.08 + waveSlope * 0.04;
        alpha = clamp(alpha, 0.58, 0.97);

        if (u_waterDebugMode == 1) {
            FragColor = vec4(vec3(alpha), 1.0);
            return;
        } else if (u_waterDebugMode == 2) {
            FragColor = vec4(vec3(waterDepth / 24.0), 1.0);
            return;
        } else if (u_waterDebugMode == 3) {
            FragColor = vec4(vec3(waterRefractionStrength), 1.0);
            return;
        } else if (u_waterDebugMode == 4) {
            FragColor = vec4(vec3(waterFresnel), 1.0);
            return;
        } else if (u_waterDebugMode == 5) {
            float rippleH = sampleRippleHeight(fs_in.FragPos.xz);
            vec3 heatColor = rippleH >= 0.0 ? vec3(0.05, 0.38, 1.0) : vec3(1.0, 0.08, 0.03);
            float heat = clamp(abs(rippleH) * 22.0, 0.0, 1.0);
            FragColor = vec4(mix(vec3(0.02), heatColor, heat), 1.0);
            return;
        } else if (u_waterDebugMode == 6) {
            float visualRipple = sampleRippleHeight(fs_in.FragPos.xz) * u_visualRippleScale;
            float visualTotal = fs_in.FragPos.y;
            vec3 signColor = visualRipple >= 0.0 ? vec3(0.05, 0.38, 1.0) : vec3(1.0, 0.08, 0.03);
            float heat = clamp(abs(visualRipple) * 16.0, 0.0, 1.0);
            FragColor = vec4(mix(vec3(clamp((visualTotal - 3.9) * 2.0, 0.0, 1.0)), signColor, heat), 1.0);
            return;
        } else if (u_waterDebugMode == 7) {
            float physicsRipple = sampleRippleHeightSmoothed(fs_in.FragPos.xz) * u_physicsRippleScale;
            vec3 signColor = physicsRipple >= 0.0 ? vec3(0.05, 0.38, 1.0) : vec3(1.0, 0.08, 0.03);
            float heat = clamp(abs(physicsRipple) * 42.0, 0.0, 1.0);
            FragColor = vec4(mix(vec3(0.02), signColor, heat), 1.0);
            return;
        }
    } else if (!isLightSource) {
        float F_fresnel = pow(1.0 - NoV, 5.0);
        alpha = mix(0.15, 0.8, F_fresnel);
        alpha = clamp(alpha * (reflectivity + 0.1), 0.05, 0.95);
    }

    FragColor = vec4(color, alpha);
}
