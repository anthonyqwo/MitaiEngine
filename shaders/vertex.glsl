#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 FragPosLightSpace;
} vs_out;

struct GerstnerWave {
    vec2 direction;
    float amplitude;
    float wavelength;
    float speed;
    float steepness;
};

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform mat4 textureMatrix;

uniform GerstnerWave waves[4];
uniform float time;
uniform bool isWater;

void main() {
    vec3 worldPos = vec3(model * vec4(aPos, 1.0));
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(vec3(model * vec4(aTangent, 0.0)));
    T = normalize(T - dot(T, N) * N);
    
    vec3 B;
    if (length(aBitangent) > 0.1) {
        B = normalize(vec3(model * vec4(aBitangent, 0.0)));
    } else {
        B = cross(N, T);
    }
    
    if (isWater) {
        float x = worldPos.x;
        float z = worldPos.z;
        float dx = 0.0;
        float dy = 0.0;
        float dz = 0.0;
        
        float tx_x = 1.0;
        float tx_y = 0.0;
        float tx_z = 0.0;
        
        float tz_x = 0.0;
        float tz_y = 0.0;
        float tz_z = 1.0;
        
        for (int i = 0; i < 4; i++) {
            vec2 d = normalize(waves[i].direction);
            float A = waves[i].amplitude;
            float L = waves[i].wavelength;
            float k = 2.0 * 3.14159265359 / L;
            float c = waves[i].speed;
            float q = waves[i].steepness / (A * k * 4.0);
            
            float theta = k * dot(d, vec2(x, z)) - c * k * time;
            
            dx += q * A * d.x * cos(theta);
            dy += A * sin(theta);
            dz += q * A * d.y * cos(theta);
            
            float s = sin(theta);
            float cosVal = cos(theta);
            
            tx_x -= q * A * k * d.x * d.x * s;
            tx_y += A * k * d.x * cosVal;
            tx_z -= q * A * k * d.x * d.y * s;
            
            tz_x -= q * A * k * d.x * d.y * s;
            tz_y += A * k * d.y * cosVal;
            tz_z -= q * A * k * d.y * d.y * s;
        }
        
        worldPos = vec3(x + dx, worldPos.y + dy, z + dz);
        
        // Reconstruct T, B, N in world space
        T = normalize(vec3(tx_x, tx_y, tx_z));
        B = normalize(vec3(tz_x, tz_y, tz_z));
        N = normalize(cross(T, B));
        
        T = normalize(T - dot(T, N) * N);
        B = cross(N, T);
    }
    
    vs_out.FragPos = worldPos;
    vs_out.TexCoords = vec2(textureMatrix * vec4(aTexCoords, 0.0, 1.0));
    vs_out.Normal = N;
    vs_out.Tangent = T;
    vs_out.Bitangent = B;
    
    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}
