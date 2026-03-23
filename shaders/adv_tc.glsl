#version 450 core
layout (vertices = 3) out;
in vec3 vNormal[];
out vec3 tcNormal[];
uniform float tessLevel;
void main() {
    tcNormal[gl_InvocationID] = vNormal[gl_InvocationID];
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = tessLevel;
        gl_TessLevelOuter[1] = tessLevel;
        gl_TessLevelOuter[2] = tessLevel;
        gl_TessLevelInner[0] = tessLevel;
    }
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
