#version 450 core
layout (triangles, equal_spacing, ccw) in;

in vec3 tcNormal[];

out vec3 teNormal;
out vec3 teFragPos;
out vec4 teFragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main() {
    vec3 p0 = gl_TessCoord.x * gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_TessCoord.y * gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_TessCoord.z * gl_in[2].gl_Position.xyz;
    vec3 pos = normalize(p0 + p1 + p2); 
    
    // 世界空間計算
    vec4 worldPos = model * vec4(pos, 1.0);
    teFragPos = worldPos.xyz;
    teNormal = mat3(transpose(inverse(model))) * pos;
    teFragPosLightSpace = lightSpaceMatrix * worldPos;
    
    // 輸出到 Geometry Shader
    gl_Position = worldPos;
}
