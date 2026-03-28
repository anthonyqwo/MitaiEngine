#version 450 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec4 gPBR;
layout (location = 4) out vec4 gEmissive;

// TES_OUT
in vec4 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

layout(binding = 10) uniform sampler2D albedoMap;
layout(binding = 11) uniform sampler2D normalMap;
layout(binding = 12) uniform sampler2D metallicMap;
layout(binding = 13) uniform sampler2D roughnessMap;
layout(binding = 14) uniform sampler2D aoMap;

uniform vec3 objectColor; 
uniform bool useNormalMap;

uniform float roughness; 
uniform float metallic;  
uniform float reflectivity;

struct Material { float ambientStrength; };
uniform Material material;

void main() {
    vec2 TexCoords = vec2(0.5, 0.5);
    vec3 Tangent = vec3(1.0, 0.0, 0.0);
    float m = texture(metallicMap, TexCoords).b * metallic;
    float r = texture(roughnessMap, TexCoords).g * roughness;
    float aoSample = texture(aoMap, TexCoords).r * material.ambientStrength;
    r = clamp(r, 0.04, 1.0);

    vec3 N_geom = normalize(Normal);
    vec3 N = N_geom;
    if(useNormalMap) {
        vec3 T = normalize(Tangent);
        T = normalize(T - dot(T, N_geom) * N_geom);
        vec3 B = cross(N_geom, T);
        mat3 TBN = mat3(T, B, N_geom);
        vec3 tangentNormal = texture(normalMap, TexCoords).rgb * 2.0 - 1.0;
        tangentNormal.xy *= 1.5;
        N = normalize(TBN * normalize(tangentNormal));
    }
    
    vec3 texAlbedo = texture(albedoMap, TexCoords).rgb;
    vec3 baseColor = pow(texAlbedo, vec3(2.2)) * objectColor;
    
    gPosition = vec4(FragPos.xyz, 1.0);
    gNormal = vec4(N, reflectivity);
    gAlbedo = vec4(baseColor, 1.0);
    gPBR = vec4(m, r, aoSample, 0.0);
    gEmissive = vec4(0.0, 0.0, 0.0, 1.0);
}
