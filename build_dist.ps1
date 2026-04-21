# MitaiEngine Distribution Build Script (小體積封裝專用)
# 這個腳本會暫時套用 CMakeLists_dist.txt 進行編譯，然後還原正常環境。
Write-Host "========================================" -ForegroundColor Magenta
Write-Host "  Starting Distribution Build (Min Size)  " -ForegroundColor Magenta
Write-Host "========================================" -ForegroundColor Magenta

# 1. 備份原本的 CMakeLists.txt
if (Test-Path "CMakeLists.txt") {
    Copy-Item "CMakeLists.txt" "CMakeLists.txt.bak" -Force
}

try {
    # 2. 應用 Distribution 的 CMakeLists
    Copy-Item "CMakeLists_dist.txt" "CMakeLists.txt" -Force

    # 清理舊的 Dist Cache
    if (Test-Path build_dist) {
        Write-Host "Cleaning old build_dist folder..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force build_dist
    }
    New-Item -ItemType Directory -Force -Path build_dist | Out-Null
    
    Write-Host "`n--- 1. Generating Release Build Files ---" -ForegroundColor Cyan
    cmake -G Ninja -B build_dist -DCMAKE_BUILD_TYPE=MinSizeRel
    
    if ($LASTEXITCODE -ne 0) { throw "CMake generation failed!" }
    
    Write-Host "`n--- 2. Building Project (LTO & Strip Enabled) ---" -ForegroundColor Cyan
    cmake --build build_dist
    
    if ($LASTEXITCODE -ne 0) { throw "Build failed!" }
    
    Write-Host "`n--- 3. Analyzing Final Output ---" -ForegroundColor Cyan
    $exePath = "./build_dist/bin/GameEngine.exe"
    
    # 嘗試呼叫外部 strip (如果環境有支援 gcc/clang 工具鏈的話) 進行最終極限瘦身
    try { strip --strip-all $exePath 2>$null } catch { }

    if (Test-Path $exePath) {
        $sizeBytes = (Get-Item $exePath).Length
        $sizeMB = [math]::Round($sizeBytes / 1MB, 2)
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "  Build Complete! " -ForegroundColor Green
        Write-Host "  EXE Path: $exePath" -ForegroundColor Green
        Write-Host "  Final Size: $sizeMB MB" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "  (此版本已移除 Console 終端機會自動以視窗模式啟動)" -ForegroundColor Yellow
    }
} catch {
    Write-Error $_
} finally {
    # 4. 還原原本的 CMakeLists.txt (超級重要，避免干擾使用者日常開發)
    if (Test-Path "CMakeLists.txt.bak") {
        Copy-Item "CMakeLists.txt.bak" "CMakeLists.txt" -Force
        Remove-Item "CMakeLists.txt.bak" -Force
        Write-Host "`n[Normal environment restored]" -ForegroundColor DarkGray
    }
}
