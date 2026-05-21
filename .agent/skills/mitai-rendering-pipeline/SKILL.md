---
name: mitai-rendering-pipeline
description: Use for MitaiEngine rendering pipeline work, including OpenGL 4.5 frame flow, Renderer.cpp changes, deferred rendering, G-Buffer attachments, shadow pass, forward transparent pass, skybox pass, render ordering, viewport handling, and diagnosing blank/incorrect frames.
---

# Skill: Mitai Rendering Pipeline

## Goal
Maintain or extend MitaiEngine's main OpenGL rendering architecture without breaking pass order, framebuffer state, or transparency/shadow behavior.

## Key Files
- `src/Renderer.cpp`
- `include/Renderer.h`
- `src/Application.cpp`
- `include/Entity.h`
- `shaders/deferred_lighting_f.glsl`
- `shaders/gbuffer_f.glsl`
- `shaders/forward_water_f.glsl`

## Pipeline Order
1. Shadow pass: render directional and point-light depth maps.
2. Geometry pass: render opaque scene data into the G-Buffer.
3. Deferred lighting pass: combine G-Buffer, PBR lights, IBL, and shadows.
4. Depth blit: copy G-Buffer depth to the default framebuffer.
5. Forward pass: render skybox, unlit lights, particles, water, glass/debug elements.
6. UI pass: ImGui is rendered after the scene.

## Workflow
1. Locate the affected pass before editing.
2. Check FBO bindings, viewport size, depth state, blend state, culling state, and texture units.
3. Keep opaque objects in the G-Buffer path unless transparency, refraction, or special blending is required.
4. Keep water and transparent surfaces in the forward pass.
5. If adding a G-Buffer channel, update setup, shader outputs, lighting shader samplers, texture bindings, and debug visualization together.
6. If adding a render mode, expose it through `Application` and ImGui only after the rendering path works.

## Constraints
- Mutual Exclusion Rendering: The entity renderer must enforce partition logic. Shaders containing the hasTessellation flag are strictly limited to ADV_SPHERE or PARTICLE types using GL_PATCHES mode. Standard shaders must skip these types to prevent GPU driver crashes from draw-mode mismatch.
- Water Pass Reflections & Normal Perturbation: Transparent water surface rendering must be activated by the isWater flag. The shader must implement dual-scrolling normals and dynamic reflectivity modulation for physical realism.
- Normal Mapping & TBN Space: Normal map perturbation must occur in World Space. The Tangent, Bitangent, and Normal (TBN) vectors must be strictly orthogonalized using Gram-Schmidt or dynamic normalization to prevent skewing direct or IBL specular lighting.
- Do not mix transparent water/glass into the deferred opaque lighting path.
- Restore OpenGL state after temporary changes such as culling, blending, depth mask, or depth function.
- Keep texture unit assignments consistent across shader setup and render-time binding.
- Prefer existing `ResourceManager::getShader` and `ResourceManager::getTexture` patterns.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Use F3 G-Buffer visualization when checking geometry data.
- Use F2 GPU profiling when evaluating pass cost.
