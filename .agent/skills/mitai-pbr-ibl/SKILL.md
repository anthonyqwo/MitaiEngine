---
name: mitai-pbr-ibl
description: Use for MitaiEngine physically based rendering and image-based lighting tasks, including Cook-Torrance BRDF, GGX, Fresnel, metallic-roughness materials, normal maps, IBL baking, irradiance maps, prefilter maps, BRDF LUT, environment reflections, tone mapping, and material tuning.
---

# Skill: Mitai PBR and IBL

## Goal
Preserve physically plausible material response while extending PBR materials, lighting, or environment reflection.

## Key Files
- `shaders/deferred_lighting_f.glsl`
- `shaders/gbuffer_f.glsl`
- `shaders/fragment.glsl`
- `include/Entity.h`
- `include/Model.h`
- `include/IBLBaker.h`
- `src/Application.cpp`

## Core Model
Use the existing metallic-roughness workflow:
- Albedo controls base color.
- Metallic blends dielectric diffuse response into conductive specular response.
- Roughness controls GGX lobe width and environment prefilter level.
- AO/ambient reduces indirect contribution.
- Reflectivity is a project-specific artistic multiplier and should not replace Fresnel.

## Workflow
1. Identify whether the change belongs in G-Buffer encoding, deferred lighting, model loading, or UI controls.
2. Keep material data encoded consistently between `gbuffer_f.glsl` and `deferred_lighting_f.glsl`.
3. Use Cook-Torrance terms already present in the shaders: GGX NDF, Smith geometry, Schlick Fresnel.
4. Bind IBL resources through `irradianceMap`, `prefilterMap`, and `brdfLUT`.
5. If adding texture types, update `Entity`, model texture loading, G-Buffer writes, shader samplers, and fallback textures.
6. Keep lighting in linear space and apply tone mapping/gamma correction only at the final output.

## Constraints
- Specular Primacy: Analytical direct specular (L1, L2 highlights) must be additively combined with IBL specular reflections rather than blended. High specular highlights on metallic surfaces must strongly pierce environment reflections.
- Metal Tinting: Specular reflection color must be modulated by the fresnelSchlick F term, where metallic specular tints with Albedo, and non-metallic specular retains original source color.
- Energy Conservation: The diffuse contribution must decrease proportionally as metallic increases, satisfying the formula kD = 1.0 - metallic.
- Fixed Light Slots: Lights must be bound to fixed shader uniform slots based on explicit Entity names (Main Sun to light1, Point Light to light2) rather than sequentially extracted.
- Do not double-apply gamma correction.
- Do not treat metallic maps as albedo or roughness maps.
- Do not remove direct analytical lights when changing IBL; direct highlights and environment reflections are combined.
- Avoid hard-coded material hacks unless they are exposed as debug/tuning controls.

## Validation
- Check metal, dielectric, rough, and glossy materials.
- Verify the Damaged Helmet, cargo metal, painted box, and wood plank assets still respond differently.
- Build with `cmake --build build_dist --config MinSizeRel`.
