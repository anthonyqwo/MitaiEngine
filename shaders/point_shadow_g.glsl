#version 450 core
layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 shadowMatrices[6];

in VS_OUT {
    vec4 FragPos;
    vec2 TexCoords;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 FragPosLightSpace;
} gs_in[];

out vec4 FragPos; 

void main() {
    for(int face = 0; face < 6; ++face) {
        gl_Layer = face; 
        for(int i = 0; i < 3; ++i) {
            FragPos = gs_in[i].FragPos;
            gl_Position = shadowMatrices[face] * FragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
