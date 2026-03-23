#version 450 core
layout (isolines, equal_spacing, point_mode) in;

uniform mat4 model;
uniform float time;
uniform float spread;

// 簡單的偽隨機函數
float hash(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    vec3 p0 = gl_in[0].gl_Position.xyz;
    // 使用 y 座標作為粒子的唯一 ID (0.0 ~ 1.0)
    float id = gl_TessCoord.y;

    // 讓每個粒子有不同的生命週期與速度
    float speed = 1.0 + hash(id) * 2.0;
    float life = fract(time * 0.2 + id); // 0.0 ~ 1.0 循環
    
    // 螺旋狀上升路徑
    float angle = id * 6.28 + time * 0.5;
    vec3 offset = vec3(
        cos(angle) * spread * life,
        life * 5.0, // 向上飄移 5 個單位
        sin(angle) * spread * life
    );

    gl_Position = model * vec4(p0 + offset, 1.0);
}
