# MitaiEngine Agent Guide

## Project Snapshot
MitaiEngine is a C++17 / OpenGL 4.5 real-time rendering and simulation engine. The existing project focus is a PBR deferred renderer with IBL, dynamic water rendering, GPU ripple simulation, collision demos, and a 3D buoyancy simulation with interactive debug tooling.

The current assignment target is HW9 AI Engine: build an autonomous hunting game in a large house with two prey types, two predator types, visual-sensor perception, AI behavior, scoring rounds, and an explanatory report. Track implementation work in `AI_ENGINE_TODO.md`.

Use this file as the first-stop orientation guide for agents. Load detailed workflow rules from `.agent/skills/<skill-name>/SKILL.md` only when the task matches that domain.

## Workspace Skills
Project-specific Antigravity skills live in:

```text
.agent/skills/
```

Available skills:

| Skill | Use When |
| --- | --- |
| `mitai-ai-engine` | HW9 AI hunting game behavior, prey/predator agents, FSMs, visual sensors, steering, capture scoring, round management. |
| `mitai-ai-navigation` | House navigation grid, blockers, valid spawning, A* pathfinding, line of sight, FOV tests, path/vision debug drawing. |
| `mitai-ai-assignment-report` | HW9 report/document explaining prey and predator AI models, visual sensors, FSMs, pathfinding, scoring, and results. |
| `mitai-rendering-pipeline` | Renderer architecture, OpenGL frame flow, deferred rendering, G-Buffer, pass ordering, viewport/FBO/state bugs. |
| `mitai-pbr-ibl` | PBR materials, Cook-Torrance BRDF, GGX, Fresnel, metallic-roughness, IBL, irradiance/prefilter/BRDF LUT, tone mapping. |
| `mitai-shader-authoring` | GLSL shader edits, shader interfaces, uniforms, samplers, tessellation, geometry shaders, compute shaders, shader compile/link issues. |
| `mitai-water-ripples` | Water rendering, Gerstner waves, water normals, Fresnel/reflection/specular, compute ripple textures, ripple impulses, water debug modes. |
| `mitai-buoyancy-physics` | Floating bodies, submerged volume, center of buoyancy, torque, open-box flooding, cargo offset, tank wall contacts, buoyancy scenarios. |
| `mitai-collision-physics` | Collision demo, sphere-AABB, sphere-sphere, spatial grid broad phase, collision count metrics, restitution/friction/substeps. |
| `mitai-model-assets` | Assimp model loading, GLTF/OBJ, textures, PBR asset mapping, embedded textures, local bounds, asset path issues. |
| `mitai-runtime-debugging` | ImGui panels, debug overlays, F1-F5 hotkeys, GPU profiler, G-Buffer visualization, water debug views, mouse grabbing. |
| `mitai-build-release` | CMake, Ninja/MinGW build issues, FetchContent dependencies, build scripts, packaging, release verification. |

Example explicit calls:

```text
@mitai-ai-engine Implement predator scoring rounds for HW9.
@mitai-ai-navigation Add A* pathfinding for house rooms.
@mitai-ai-assignment-report Draft the AI model explanation.
@mitai-buoyancy-physics Adjust open-box flooding stability.
@mitai-water-ripples Debug ripple height artifacts.
@mitai-rendering-pipeline Add a new G-Buffer visualization mode.
@mitai-build-release Verify the distribution build.
```

## Core Architecture
Main application flow:

1. `src/main.cpp` creates `Application`.
2. `Application::init` initializes GLFW, GLAD, ImGui, resources, renderer, models, skybox, and IBL.
3. `Application::run` processes input, updates scene/physics, renders the scene, then renders ImGui.
4. `Renderer::renderScene` performs shadow, G-Buffer, deferred lighting, forward transparent/water, debug, and skybox passes.

Important files:

```text
src/Application.cpp        Runtime setup, scene loading, input, ImGui.
src/Renderer.cpp           Rendering passes, debug drawing, GPU timings.
src/PhysicsSystem.cpp      Collision, buoyancy, flooding, water impulses.
src/RippleSystem.cpp       GPU ripple compute texture simulation.
src/Scene.cpp              Scene update and collision-aware camera movement.
include/Entity.h           Entity data model for rendering, physics, lights, debug.
include/Model.h            Assimp model loading and PBR texture extraction.
include/WaterWaves.h       Shared procedural water wave parameters.
shaders/                   GLSL shader pipeline.
assets/                    Textures, PBR sets, skybox, GLB models.
```

## Rendering Pipeline
Render order matters:

1. Directional and point-light shadow maps.
2. Opaque geometry into G-Buffer.
3. Deferred PBR lighting with IBL and shadows.
4. Depth blit from G-Buffer to default framebuffer.
5. Forward skybox, unlit light markers, particles, water/glass/debug.
6. ImGui.

Do not move transparent water into the opaque deferred path. If a shader interface changes, update C++ resource loading, uniform binding, and all matching shader variants.

## Physics and Water
Buoyancy is the current flagship simulation area.

- Sphere, slab, and open-top box buoyancy are handled in `PhysicsSystem`.
- Buoyant force follows submerged volume and water density.
- Torque comes from center-of-buoyancy versus center-of-mass offset.
- Open-top boxes use `floodLevel` for ingress/drainage and sinking behavior.
- Water ripples are generated from physical interaction impulses and evolved by compute shaders.

Keep visual water and physics water sampling aligned. Changes to wave height/normal logic often require updates in both `forward_water_f.glsl` and `wave_query.glsl`.

## HW9 AI Engine Target
The AI assignment should prioritize autonomous behavior over rendering polish.

Required AI pieces:

- House scene with rooms, walls, doors, and furniture/obstacles.
- 20 preys at round start: 10 green and 10 blue.
- Different prey values; higher-value prey must move faster.
- Two predator types with different visual identity and AI strategy.
- Visual-sensor perception based on range, FOV, and line of sight.
- Prey AI that wanders and flees when predators are visually detected.
- Predator AI that searches, chases, captures, and scores prey.
- Fair round manager: predator 1 round, reset, predator 2 round, winner.
- Debug/UI support for timer, scores, states, paths, and vision.

Recommended routing:

- Start from `AI_ENGINE_TODO.md`.
- Use `mitai-ai-engine` for behavior and scoring.
- Use `mitai-ai-navigation` for perception, grid, A*, and blockers.
- Use `mitai-runtime-debugging` for UI and overlays.
- Use `mitai-ai-assignment-report` when preparing the grading document.

## Runtime Controls
Useful controls:

```text
Tab       Toggle mouse look / cursor.
W/A/S/D   Move camera.
Space     Move camera up.
Shift     Move camera down.
Ctrl      Sprint.
F1        Toggle buoyancy debug overlay.
F2        Toggle GPU profiling overlay.
F3        Cycle G-Buffer visualization.
F4        Toggle water waves.
F5        Cycle water debug mode.
1-4       Load buoyancy scenarios when cursor is active.
```

ImGui panels include scene hierarchy, inspector, engine controls, collision controls, water tuning, instructions, and profiling/debug overlays.

## Build and Verification
Preferred verification command:

```powershell
cmake --build build_dist --config MinSizeRel
```

Known build note:

- `build_dist` is the reliable Ninja distribution build directory.
- `build` may contain generator/cache mismatch symptoms. Inspect `build/CMakeCache.txt` before relying on it.
- Do not delete build directories or run destructive clean commands unless explicitly asked.

After build, expected executable:

```text
build_dist/bin/GameEngine.exe
```

## Working Rules for Agents
- Read the relevant skill before making domain-specific edits.
- Prefer `rg` / `rg --files` for search.
- Keep changes scoped to the requested system.
- Use existing patterns: `ResourceManager`, `Entity`, ImGui state in `Application`, render logic in `Renderer`, physics logic in `PhysicsSystem`.
- Preserve user changes. Do not revert unrelated files.
- Build with `build_dist` after code or shader changes when feasible.
- Update instructions/debug UI if adding or changing hotkeys.
- For shader changes, consider visible pass, shadow pass, debug mode, and compute path together.
- For physics changes, test scenarios and keep forces/torques bounded.

## Common Task Routing
- HW9 hunting game implementation: load `mitai-ai-engine`, then read `AI_ENGINE_TODO.md`.
- House pathfinding or visual sensors: load `mitai-ai-navigation`, then inspect obstacles, grid, FOV, and line of sight.
- HW9 written document/report: load `mitai-ai-assignment-report`, then align claims to implemented behavior.
- Rendering artifact or blank frame: load `mitai-rendering-pipeline`, then inspect FBO/state/pass order.
- Strange material or reflection: load `mitai-pbr-ibl`, then check G-Buffer material channels and IBL bindings.
- Shader compile/link issue: load `mitai-shader-authoring`, then inspect shader stage interfaces and C++ uniform bindings.
- Water visual bug: load `mitai-water-ripples`, then compare renderer water shader and wave query shader.
- Floating/sinking instability: load `mitai-buoyancy-physics`, then inspect submerged volume, force clamp, torque, and damping.
- Collision count/performance issue: load `mitai-collision-physics`, then compare spatial grid and exhaustive paths.
- Missing model/texture: load `mitai-model-assets`, then inspect `AssetPath`, `Model`, `Mesh`, and `ResourceManager`.
- UI/debug feature: load `mitai-runtime-debugging`, then update hotkeys, ImGui, and debug rendering together.
- Build/package issue: load `mitai-build-release`, then inspect CMake cache, scripts, and output layout.
