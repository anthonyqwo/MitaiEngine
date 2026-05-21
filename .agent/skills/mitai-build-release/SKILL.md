---
name: mitai-build-release
description: Use for MitaiEngine build, packaging, dependency, and release workflow tasks, including CMake, Ninja, MinGW, FetchContent dependencies, build.ps1, build_dist.ps1, build_release.ps1, executable output directories, diagnosing generator/cache mismatches, and verifying GameEngine.exe builds.
---

# Skill: Mitai Build and Release

## Goal
Build and package MitaiEngine reliably while respecting the existing Windows/CMake workflow.

## Key Files
- `CMakeLists.txt`
- `CMakeLists_dist.txt`
- `build.ps1`
- `build_dist.ps1`
- `build_release.ps1`
- `.gitignore`
- `build/`
- `build_dist/`
- `build_release/`

## Current Build Notes
- The project uses C++17.
- Dependencies are fetched through CMake FetchContent: GLFW, GLM, ImGui, Assimp.
- `build_dist` is known to build successfully with Ninja.
- `build` may contain a generator/cache mismatch; do not assume `cmake --build build` works.

## Workflow
1. Prefer `cmake --build build_dist --config MinSizeRel` for verification unless the user asks for another target.
2. Inspect `CMakeCache.txt` when a build directory fails because of generator mismatch.
3. Keep dependency settings conservative, especially Assimp importer/exporter options.
4. Do not delete build directories unless explicitly requested.
5. For release packaging, verify the executable and required assets/shaders are included.
6. Keep generated binaries and build artifacts out of source changes.

## Constraints
- Do not run destructive clean commands without explicit approval.
- Do not change dependency versions unless needed for the task.
- Do not assume network access is available for dependency downloads.

## Validation
- Run `cmake --build build_dist --config MinSizeRel`.
- Confirm `build_dist/bin/GameEngine.exe` exists after build.
- If packaging, launch or inspect the packaged directory structure for `assets/` and `shaders/`.
