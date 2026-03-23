#version 450 core
out vec4 FragColor;
in vec2 TexCoord;
in float fragLife;

uniform vec3 objectColor;

void main() {
    float dist = distance(TexCoord, vec2(0.5));
    if(dist > 0.5) discard;
    
    // 圓形邊緣羽化
    float shapeAlpha = smoothstep(0.5, 0.2, dist);
    
    // 強化生命週期衰減：
    // 1. 使用 smoothstep 確保在 life > 0.8 時加速淡出，並在 1.0 時絕對為 0
    float fadeOut = smoothstep(1.0, 0.7, fragLife);
    // 2. 結合原本的 pow 衰減，讓整體過程更柔和
    float lifeAlpha = pow(fadeOut, 2.0);
    
    FragColor = vec4(objectColor, shapeAlpha * lifeAlpha);
}
