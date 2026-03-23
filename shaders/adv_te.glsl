#version 450 core
layout (triangles, equal_spacing, ccw) in;
in vec3 tcNormal[];
out vec3 teNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vec3 p0 = gl_TessCoord.x * gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_TessCoord.y * gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_TessCoord.z * gl_in[2].gl_Position.xyz;
    vec3 pos = normalize(p0 + p1 + p2); // 強制投影到單位球面上
    
    teNormal = pos; // 對於圓心在原點的球體，位置即法線
    gl_Position = projection * view * model * vec4(pos, 1.0);
}
