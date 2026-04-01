#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 aBoneIDs;
layout (location = 6) in vec4 aWeights;

out VS_OUT {
    vec4 FragPos;
    vec2 TexCoords;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 FragPosLightSpace;
} vs_out;

const int MAX_BONES = 2000;
const int MAX_BONE_INFLUENCE = 4;
layout(std430, binding = 0) buffer BoneMatrices {
    mat4 finalBonesMatrices[];
};

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform mat4 textureMatrix;

void main() {
    vec4 totalPosition = vec4(0.0);
    vec3 totalNormal = vec3(0.0);
    
    float validWeightSum = 0.0;
    bool hasBones = false;
    
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        if(aBoneIDs[i] < 0 || aBoneIDs[i] >= MAX_BONES) continue;
        
        hasBones = true;
        validWeightSum += aWeights[i];
        
        vec4 localPosition = finalBonesMatrices[aBoneIDs[i]] * vec4(aPos, 1.0);
        totalPosition += localPosition * aWeights[i];
        
        mat3 boneNormalMatrix = mat3(finalBonesMatrices[aBoneIDs[i]]);
        totalNormal += boneNormalMatrix * aNormal * aWeights[i];
    }
    
    if(!hasBones || validWeightSum < 0.01) {
        totalPosition = finalBonesMatrices[0] * vec4(aPos, 1.0);
        mat3 boneNormalMatrix = mat3(finalBonesMatrices[0]);
        totalNormal = boneNormalMatrix * aNormal;
    } else {
        totalPosition /= validWeightSum;
    }

    vs_out.FragPos = model * totalPosition; 
    vs_out.TexCoords = vec2(textureMatrix * vec4(aTexCoords, 0.0, 1.0));
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * totalNormal);
    vec3 T = normalize(vec3(model * vec4(aTangent, 0.0)));
    T = normalize(T - dot(T, N) * N);
    
    vec3 B = cross(N, T);
    
    vs_out.Normal = N;
    vs_out.Tangent = T;
    vs_out.Bitangent = B;
    
    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos.xyz, 1.0);
    gl_Position = projection * view * vec4(vs_out.FragPos.xyz, 1.0);
}
