#version 450 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;
in vec3 teNormal[];
out vec3 gNormal;
uniform float explosionFactor;
uniform float time;

vec4 explode(vec4 position, vec3 normal) {
    float magnitude = 2.0;
    vec3 direction = normal * explosionFactor * magnitude;
    return position + vec4(direction, 0.0);
}

vec3 getNormal() {
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    return normalize(cross(a, b));
}

void main() {
    vec3 normal = getNormal();
    for(int i = 0; i < 3; i++) {
        gl_Position = explode(gl_in[i].gl_Position, normal);
        gNormal = teNormal[i];
        EmitVertex();
    }
    EndPrimitive();
}
