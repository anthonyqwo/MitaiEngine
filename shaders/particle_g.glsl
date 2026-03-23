#version 450 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in float lifeFactor[];

uniform mat4 projection;
uniform mat4 view;
uniform float size;

out vec2 TexCoord;
out float fragLife;

void main() {
    float life = lifeFactor[0];
    fragLife = life;

    // 隨生命週期放大粒子 (1.0x -> 4.0x)
    float dynamicSize = size * (1.0 + life * 3.0);

    vec3 center = gl_in[0].gl_Position.xyz;
    vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 up    = vec3(view[0][1], view[1][1], view[2][1]);
    
    vec3 p[4];
    p[0] = center - (right + up) * dynamicSize;
    p[1] = center + (right - up) * dynamicSize;
    p[2] = center - (right - up) * dynamicSize;
    p[3] = center + (right + up) * dynamicSize;
    
    vec2 uv[4] = { vec2(0,0), vec2(1,0), vec2(0,1), vec2(1,1) };
    for(int i=0; i<4; i++) {
        gl_Position = projection * view * vec4(p[i], 1.0);
        TexCoord = uv[i];
        EmitVertex();
    }
    EndPrimitive();
}
