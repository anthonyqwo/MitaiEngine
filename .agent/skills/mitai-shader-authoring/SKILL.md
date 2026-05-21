---
name: mitai-shader-authoring
description: Use for MitaiEngine GLSL shader work, including vertex, fragment, geometry, tessellation, compute shaders, shader interface matching, uniform binding, texture samplers, advanced sphere tessellation, synchronized shadow shaders, particle shaders, and shader compile/link debugging.
---

# Skill: Mitai Shader Authoring

## Goal
Modify GLSL shaders and C++ shader bindings together so shader interfaces remain valid across render passes.

## Key Files
- `shaders/*.glsl`
- `src/ResourceManager.cpp`
- `include/Shader.h`
- `src/Application.cpp`
- `src/Renderer.cpp`

## Shader Families
- G-Buffer: `vertex.glsl`, `gbuffer_f.glsl`
- Advanced tessellation: `adv_v.glsl`, `adv_tc.glsl`, `adv_te.glsl`, `adv_g.glsl`, `adv_gbuffer_f.glsl`
- Shadows: `shadow_*`, `point_shadow_*`, `adv_*shadow*`
- Deferred lighting: `deferred_lighting_v.glsl`, `deferred_lighting_f.glsl`
- Water: `forward_water_f.glsl`, `gerstner_common.glsl`, `ripple_common.glsl`
- Compute: `wave_query.glsl`, `ripple_step.glsl`, `ripple_inject.glsl`
- Particles: `particle_*`

## Workflow
1. Find all C++ load calls for the shader before changing stage inputs or uniforms.
2. Keep `in`/`out` block names, locations, and types matched between stages.
3. When adding uniforms, bind them in `Renderer.cpp` or `Application.cpp` near similar uniforms.
4. When changing tessellated geometry, update advanced G-Buffer and advanced shadow paths together.
5. When changing compute shaders, check image formats, binding indices, SSBO layout, barriers, and dispatch group size.
6. Prefer shared include-like common GLSL files already used by the project for water/wave logic.

## Constraints
- Adv-Shader Interface: All FragPos variables involved in the adv_shader (Tessellation/Geometry) pipeline must be declared as vec4. This guarantees interface compatibility with point_shadow_f.glsl to avoid link errors. Cast or swizzle to .xyz inside the Fragment Shader where position calculations occur.
- Displacement & Shadow Sync: Tessellated or displaced geometry (e.g., wave displacement, explosion) must render its shadow pass using advanced shadow shaders (advShadowShader, advPointShadowShader) to prevent visual-shadow separation.
- Explosion Vector Calculation: Geometry explosion displacement direction must be calculated using the arithmetic mean of vertex normals. Outward displacement must be strictly positive, and unstable triangle cross-products are forbidden.
- Avoid changing only the visible shader when a shadow/depth equivalent exists.
- Keep sampler units consistent with C++ binding code.
- Do not introduce shader outputs without updating framebuffer attachments.
- Preserve OpenGL 4.5 compatibility.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- If shader expansion/check files exist in `build`, compare generated/expanded shader code only as diagnostic output.
- Exercise the affected scene and use debug modes for G-Buffer or water where relevant.
