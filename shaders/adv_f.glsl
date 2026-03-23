#version 450 core
out vec4 FragColor;
in vec3 gNormal;
uniform vec3 objectColor;
void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(normalize(gNormal), lightDir), 0.2);
    FragColor = vec4(objectColor * diff, 1.0);
}
