#version 450 core
out vec4 FragColor;
in vec2 TexCoord;
uniform vec3 objectColor;
void main() {
    float dist = distance(TexCoord, vec2(0.5));
    if(dist > 0.5) discard;
    // 使用漸變邊緣，但中心保持亮度
    float alpha = smoothstep(0.5, 0.2, dist);
    FragColor = vec4(objectColor, alpha);
}
