#version 450 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
out VS_OUT {
    vec4 FragPos;
    vec2 TexCoords;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 FragPosLightSpace;
} vs_out;

void main() {
    vs_out.FragPos = model * vec4(aPos, 1.0);
    gl_Position = vs_out.FragPos;
    
    // Initialize others to 0 to avoid undefined values reaching GS
    vs_out.TexCoords = vec2(0.0);
    vs_out.Normal = vec3(0.0, 1.0, 0.0);
    vs_out.Tangent = vec3(1.0, 0.0, 0.0);
    vs_out.Bitangent = vec3(0.0, 0.0, 1.0);
    vs_out.FragPosLightSpace = vec4(0.0);
}
