#version 450 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

in vec3 teNormal[];
in vec4 teFragPos[];

out vec4 FragPos; // 點陰影計算所需的輸出

uniform mat4 shadowMatrices[6];
uniform float explosionFactor;

void main() {
    // 爆炸位移邏輯 (必須與 adv_g.glsl 一致)
    vec3 explodeDir = normalize(teNormal[0] + teNormal[1] + teNormal[2]);
    float magnitude = 2.0;
    vec3 offset = explodeDir * explosionFactor * magnitude;

    for(int face = 0; face < 6; ++face) {
        gl_Layer = face; // 設定渲染到 Cubemap 的第幾面
        for(int i = 0; i < 3; ++i) {
            vec3 worldPos = teFragPos[i].xyz + offset;
            FragPos = vec4(worldPos, 1.0);
            gl_Position = shadowMatrices[face] * FragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
