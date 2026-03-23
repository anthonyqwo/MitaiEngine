# 1. 建立資源資料夾
if (!(Test-Path assets)) { New-Item -ItemType Directory -Force -Path assets }
if (!(Test-Path assets/skybox)) { New-Item -ItemType Directory -Force -Path assets/skybox }

Write-Host "--- Downloading High-Quality Assets (Stable Links) ---" -ForegroundColor Cyan

# 2. 下載高品質木箱貼圖 (來自 LearnOpenGL 官方資源)
# Diffuse (顏色)
curl.exe -s -o assets/container.jpg https://learnopengl.com/img/textures/container2.png
# Specular (鏡面光)
curl.exe -s -o assets/container_specular.png https://learnopengl.com/img/textures/container2_specular.png
# Normal (法線 - 這是正確的版本)
curl.exe -s -o assets/container_normal.png https://learnopengl.com/img/textures/toy_box_normal.png

# 3. 下載高品質磚牆貼圖 (地板用)
curl.exe -s -o assets/wall.jpg https://learnopengl.com/img/textures/brickwall.jpg
curl.exe -s -o assets/wall_normal.jpg https://learnopengl.com/img/textures/brickwall_normal.jpg

# 4. 下載天空盒
$skybox_url = "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/resources/textures/skybox/"
$faces = "right", "left", "top", "bottom", "front", "back"
foreach ($face in $faces) {
    curl.exe -s -o "assets/skybox/$face.jpg" "$($skybox_url)$($face).jpg"
}

Write-Host "--- All Assets Ready! ---" -ForegroundColor Green
