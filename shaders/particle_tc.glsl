#version 450 core
layout (vertices = 1) out;
uniform float particleCount;

void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    
    if (gl_InvocationID == 0) {
        // 在 isolines 模式下：
        // Outer[0] 控制繪製多少條線
        // Outer[1] 控制每條線有多少段
        // 搭配 TES 的 point_mode，這裡將生成 [particleCount] 個點
        gl_TessLevelOuter[0] = particleCount;
        gl_TessLevelOuter[1] = 1.0;
    }
}
