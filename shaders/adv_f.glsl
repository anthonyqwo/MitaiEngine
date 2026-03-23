#version 450 core
out vec4 FragColor;

in vec3 Normal;
in vec4 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D shadowMap;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D   brdfLUT;
uniform samplerCube pointShadowMap;

struct Material { float ambientStrength; };
uniform Material material;

struct Light { vec3 position; vec3 color; };
uniform Light light1;
uniform Light light2;

uniform vec3 objectColor; 
uniform vec3 viewPos;
uniform float roughness;
uniform float metallic;
uniform float reflectivity;
uniform float far_plane;

const float PI = 3.14159265359;

// --- 輔助函數 ---
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

float PointShadowCalculation(vec3 fragPos, vec3 normal) {
    vec3 fragToLight = fragPos - light2.position;
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
    vec3 N = normalize(Normal);
    vec3 vFragPos = FragPos.xyz;
    vec3 V = normalize(viewPos - vFragPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 0.0);

    vec3 albedo = objectColor; 
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float specIntensity = reflectivity * 8.0; 

    vec3 directLo = vec3(0.0);
    // Light 1
    if(length(light1.color) > 0.01) {
        vec3 L = normalize(light1.position - vFragPos); vec3 H = normalize(V + L);
        float dist = length(light1.position - vFragPos);
        float atten = 1.0 / (dist * dist + 0.001);
        float shadow = ShadowCalculation(FragPosLightSpace, N, L);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, roughness); 
        float G = GeometrySmith(N, V, L, roughness); 
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * NoV * NdotL + 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        directLo += (kD * albedo / PI + spec * specIntensity) * light1.color * atten * NdotL * (1.0 - shadow) * 100.0;
    }
    // Light 2
    if(length(light2.color) > 0.01) {
        vec3 L = normalize(light2.position - vFragPos); vec3 H = normalize(V + L);
        float dist = length(light2.position - vFragPos);
        float atten = 1.0 / (dist * dist + 0.001);
        float shadow = PointShadowCalculation(vFragPos, N);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, roughness); 
        float G = GeometrySmith(N, V, L, roughness); 
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * NoV * NdotL + 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        directLo += (kD * albedo / PI + spec * specIntensity) * light2.color * atten * NdotL * (1.0 - shadow) * 100.0;
    }

    // IBL
    vec3 F_ibl = fresnelSchlickRoughness(NoV, F0, roughness);
    vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - metallic);
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;
    float lod = roughness * 4.0; 
    vec3 pref = textureLod(prefilterMap, R, lod).rgb;    
    vec2 brdf = texture(brdfLUT, vec2(NoV, roughness)).rg;
    vec3 specularIBL = pref * (F_ibl * brdf.x + brdf.y);
    
    vec3 ambient = (kD_ibl * diffuseIBL + specularIBL * specIntensity) * material.ambientStrength;
    
    vec3 color = directLo + ambient;
    color = color / (color + vec3(1.0)); 
    color = pow(color, vec3(1.0/2.2));   
    FragColor = vec4(color, 1.0);
}
