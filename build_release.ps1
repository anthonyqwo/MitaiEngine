# MitaiEngine Release Build Script
# MinSizeRel + Strip + Dead-code elimination

# Clean & create build directory
if (Test-Path build_release) {
    if (Test-Path "build_release\CMakeCache.txt") {
        Write-Host "--- Cleaning old CMake Cache ---" -ForegroundColor Yellow
        Remove-Item "build_release\CMakeCache.txt" -Force
    }
} else {
    New-Item -ItemType Directory -Force -Path build_release | Out-Null
}

Write-Host "--- 1. Generating Release Build Files ---" -ForegroundColor Cyan
cmake -G Ninja -B build_release -DCMAKE_BUILD_TYPE=MinSizeRel "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" "-DCMAKE_C_FLAGS_MINSIZEREL=-Os -DNDEBUG -s" "-DCMAKE_CXX_FLAGS_MINSIZEREL=-Os -DNDEBUG -s" "-DCMAKE_EXE_LINKER_FLAGS=-s -Wl,--gc-sections" "-DCMAKE_C_FLAGS=-ffunction-sections -fdata-sections" "-DCMAKE_CXX_FLAGS=-ffunction-sections -fdata-sections"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake generation failed!"
    exit $LASTEXITCODE
}

Write-Host "--- 2. Building Project (MinSizeRel) ---" -ForegroundColor Cyan
cmake --build build_release

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit $LASTEXITCODE
}

# Locate executable
$exePath = $null
if (Test-Path "./build_release/bin/GameEngine.exe") {
    $exePath = "./build_release/bin/GameEngine.exe"
} elseif (Test-Path "./build_release/GameEngine.exe") {
    $exePath = "./build_release/GameEngine.exe"
}

if ($exePath) {
    # Strip residual symbols (double insurance)
    Write-Host "--- 3. Stripping symbols ---" -ForegroundColor Cyan
    strip --strip-all $exePath 2>$null

    $size = [math]::Round((Get-Item $exePath).Length / 1MB, 2)
    Write-Host "--- Build Complete ---" -ForegroundColor Green
    Write-Host "EXE Size: $size MB" -ForegroundColor Green
    Write-Host "Path: $exePath" -ForegroundColor Green
} else {
    Write-Error "Executable not found!"
}
