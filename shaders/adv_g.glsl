#version 450 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in vec3 teNormal[];
in vec4 teFragPos[];
in vec4 teFragPosLightSpace[];

layout (location = 0) out vec4 FragPos;
layout (location = 1) out vec3 Normal;
layout (location = 2) out vec4 FragPosLightSpace;

uniform float explosionFactor;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 lightSpaceMatrix;
uniform bool isShadowPass;

void main() {
    // 使用頂點法線的平均值作為爆炸方向，確保絕對外擴
    vec3 explodeDir = normalize(teNormal[0] + teNormal[1] + teNormal[2]);
    float magnitude = 2.0; 
    vec3 offset = explodeDir * explosionFactor * magnitude;

    for(int i = 0; i < 3; i++) {
        vec3 worldPos = teFragPos[i].xyz + offset;
        FragPos = vec4(worldPos, 1.0);
        Normal = teNormal[i];
        
        // 重新投影光照空間座標
        FragPosLightSpace = lightSpaceMatrix * FragPos;
        
        if (isShadowPass) {
            gl_Position = lightSpaceMatrix * FragPos;
        } else {
            gl_Position = projection * view * FragPos;
        }
        EmitVertex();
    }
    EndPrimitive();
}
