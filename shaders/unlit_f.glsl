#version 450 core
out vec4 FragColor;
uniform vec3 objectColor;
void main() {
    // 搭配 HDR 與 Gamma 校正的自發光輸出
    vec3 c = objectColor * 2.5; // Emissive boost
    c = c / (c + vec3(1.0)); 
    c = pow(c, vec3(1.0/2.2));
    FragColor = vec4(c, 1.0);
}
