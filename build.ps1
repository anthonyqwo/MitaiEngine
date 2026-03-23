# 檢查 build 資料夾是否存在
if (!(Test-Path build)) {
    New-Item -ItemType Directory -Force -Path build
} else {
    if (Test-Path "build\CMakeCache.txt") {
        Write-Host "--- Cleaning old CMake Cache ---" -ForegroundColor Yellow
        Remove-Item "build\CMakeCache.txt" -Force
    }
}

Write-Host "--- 1. Generating Build Files with Ninja ---" -ForegroundColor Cyan
# 這裡務必使用引號包覆 3.5 參數
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake generation failed!"
    exit $LASTEXITCODE
}

Write-Host "--- 2. Building Project ---" -ForegroundColor Cyan
cmake --build build

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit $LASTEXITCODE
}

Write-Host "--- 3. Running Game Engine ---" -ForegroundColor Green
if (Test-Path "./build/bin/GameEngine.exe") {
    ./build/bin/GameEngine.exe
} elseif (Test-Path "./build/GameEngine.exe") {
    ./build/GameEngine.exe
} else {
    Write-Error "Executable not found!"
}