#version 450 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec4 gPBR;
layout (location = 4) out vec4 gEmissive;

in VS_OUT {
    vec4 FragPos;
    vec2 TexCoords;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 FragPosLightSpace;
} fs_in;

layout(binding = 10) uniform sampler2D albedoMap;
layout(binding = 11) uniform sampler2D normalMap;
layout(binding = 12) uniform sampler2D metallicMap;
layout(binding = 13) uniform sampler2D roughnessMap;
layout(binding = 14) uniform sampler2D aoMap;
layout(binding = 15) uniform sampler2D emissiveMap;

uniform vec3 objectColor; 
uniform bool useNormalMap;

uniform float roughness; 
uniform float metallic;  
uniform float reflectivity;

struct Material { float ambientStrength; };
uniform Material material;

void main() {
    float m = texture(metallicMap, fs_in.TexCoords).b * metallic;
    float r = texture(roughnessMap, fs_in.TexCoords).g * roughness;
    float aoSample = texture(aoMap, fs_in.TexCoords).r * material.ambientStrength;
    r = clamp(r, 0.04, 1.0);

    vec3 N_geom = normalize(fs_in.Normal);
    vec3 N = N_geom;
    if(useNormalMap) {
        vec3 T = normalize(fs_in.Tangent);
        T = normalize(T - dot(T, N_geom) * N_geom);
        vec3 B = normalize(fs_in.Bitangent);
        if (length(B) < 0.1) B = cross(N_geom, T);
        else B = normalize(B - dot(B, N_geom) * N_geom - dot(B, T) * T);
        mat3 TBN = mat3(T, B, N_geom);
        vec3 tangentNormal = texture(normalMap, fs_in.TexCoords).rgb * 2.0 - 1.0;
        tangentNormal.xy *= 1.5;
        N = normalize(TBN * normalize(tangentNormal));
    }
    
    vec3 texAlbedo = texture(albedoMap, fs_in.TexCoords).rgb;
    vec3 baseColor = pow(texAlbedo, vec3(2.2)) * objectColor;
    
    gPosition = vec4(fs_in.FragPos.xyz, 1.0);
    gNormal = vec4(N, reflectivity);
    gAlbedo = vec4(baseColor, 1.0);
    gPBR = vec4(m, r, aoSample, 0.0);
    
    vec3 emissive = pow(texture(emissiveMap, fs_in.TexCoords).rgb, vec3(2.2)) * 2.0;
    gEmissive = vec4(emissive, 1.0);
}
