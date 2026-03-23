#version 450 core
layout (isolines, equal_spacing, point_mode) in;

uniform mat4 model;
uniform float time;
uniform float spread;

out float lifeFactor;

// 簡單的偽隨機函數
float hash(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    vec3 p0 = gl_in[0].gl_Position.xyz;
    float id = gl_TessCoord.y;

    // 為每個粒子計算獨立的隨機速度 (0.5 ~ 1.5)
    float randSpeed = 0.5 + hash(id + 1.0);
    // 讓生命週期進度受隨機速度影響
    float life = fract(time * 0.15 * randSpeed + id); 
    lifeFactor = life;
    
    float angle = id * 6.28 + time * (0.3 + hash(id) * 0.5);
    // 上升高度隨隨機速度變化 (4.0 ~ 8.0 單位)
    float maxHeight = 4.0 + hash(id + 2.0) * 4.0;
    
    vec3 offset = vec3(
        cos(angle) * spread * life * (1.0 + life), // 隨高度擴散
        life * maxHeight,
        sin(angle) * spread * life * (1.0 + life)
    );

    gl_Position = model * vec4(p0 + offset, 1.0);
}
