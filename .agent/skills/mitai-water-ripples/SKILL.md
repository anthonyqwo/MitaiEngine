---
name: mitai-water-ripples
description: Use for MitaiEngine water rendering and ripple simulation work, including Gerstner waves, water normals, Fresnel/reflection/specular tuning, forward water pass, depth/refraction behavior, compute-shader ripple height fields, ripple impulse injection, water physics queries, and water debug modes.
---

# Skill: Mitai Water and Ripples

## Goal
Extend or debug the water system while keeping visual waves, ripple simulation, and physics water queries consistent.

## Key Files
- `include/WaterWaves.h`
- `include/RippleSystem.h`
- `src/RippleSystem.cpp`
- `src/PhysicsSystem.cpp`
- `src/Renderer.cpp`
- `shaders/forward_water_f.glsl`
- `shaders/wave_query.glsl`
- `shaders/ripple_step.glsl`
- `shaders/ripple_inject.glsl`
- `shaders/gerstner_common.glsl`
- `shaders/ripple_common.glsl`

## Architecture
Water combines:
- Procedural/Gerstner-style wave displacement.
- Water normal texture detail.
- GPU ripple height texture from compute shaders.
- Forward-rendered reflection, Fresnel, specular, depth, and transparency.
- Physics queries that sample the same wave/ripple model for buoyancy.

## Workflow
1. Decide whether the change affects visual water, physics water queries, ripple propagation, or impulse generation.
2. Keep `forward_water_f.glsl` and `wave_query.glsl` aligned when changing wave height or normal logic.
3. For ripple behavior, update `RippleTuning`, C++ uniform binding, and relevant compute shader constants together.
4. For physics-driven ripples, inspect impulse generation in `PhysicsSystem::update` before changing `RippleSystem`.
5. Use F4 to isolate base water without waves and F5 to cycle water debug targets.
6. Keep ripple amplitude, damping, propagation speed, and clamp values bounded to avoid unstable height fields.

## Constraints
- Physical vs. Visual Ripple Separation: The system must decouple physical fluid queries from visual rendering scales. Buoyancy calculations must rely on smoothed, filtered height samples (u_physicsRippleScale) and analytical normal estimations. Visual shaders must leverage sharper, combined normal mapping (u_visualRippleScale and u_rippleNormalStrength) for visual aesthetics.
- Triple-Buffer CFL Stability: The 2D wave equation compute step must bound the Courant-Friedrichs-Lewy (CFL) value (clamped under 0.30) to prevent numerical divergence. Always clamp dynamic deltaTime below 0.033s during the compute step updates.
- Edge Fade & Absorbing Boundaries: The 2D ripple simulation must apply edge fading within 10 texels of the boundaries to emulate perfectly matched boundaries and prevent wave reflections.
- Do not bypass `RippleSystem::bindRippleUniforms`; it centralizes texture and tuning uniforms.
- Do not let physics use a different water height model than the renderer.
- Keep ripple texture format as `GL_R32F` unless all image bindings and samplers are updated.
- Preserve the triple-buffer update model: previous, current, next.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Test Water Demo and Buoyancy Demo.
- Check water debug modes for ripple height, raw height, normals, specular, reflection, and final lighting.
