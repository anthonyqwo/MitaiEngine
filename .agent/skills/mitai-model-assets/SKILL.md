---
name: mitai-model-assets
description: Use for MitaiEngine model loading and asset pipeline work, including Assimp GLTF/OBJ import, Mesh and Model classes, texture loading, embedded textures, PBR material texture mapping, tangent/bitangent handling, asset paths, local bounds, skybox and project asset organization.
---

# Skill: Mitai Model and Asset Pipeline

## Goal
Add or fix assets, model loading, material extraction, and texture assignment without breaking existing scenes.

## Key Files
- `include/Model.h`
- `include/Mesh.h`
- `include/AssetPath.h`
- `src/ResourceManager.cpp`
- `include/ResourceManager.h`
- `src/Application.cpp`
- `assets/`

## Supported Assets
- Models: GLB/GLTF and OBJ through Assimp configuration.
- Textures: albedo, normal, metallic, roughness, AO, emissive, skybox faces.
- Demo models: Damaged Helmet, fantasy house, trees.
- PBR sets: cargo metal, painted box, wood plank.

## Workflow
1. Add assets under `assets/` using clear folders for model families or PBR material sets.
2. Resolve files through `AssetPath::resolve` or existing ResourceManager patterns.
3. When adding model texture support, update Assimp texture extraction and shader-side expectations together.
4. Ensure loaded meshes compute local bounds for collision, selection, and scene scaling.
5. Use fallback solid textures when a material channel is optional.
6. Keep scene-specific asset binding in `Application::setupResources` or scene loaders.

## Constraints
- Do not hard-code absolute asset paths.
- Do not assume all GLTF materials expose textures under the same Assimp texture type.
- Preserve embedded texture support.
- Keep model loading compatible with Windows path separators.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Test Default Demo and World Demo.
- Verify material channels appear distinct under normal lighting and G-Buffer visualization.
