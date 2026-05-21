---
name: mitai-buoyancy-physics
description: Use for MitaiEngine buoyancy simulation work, including floating bodies, submerged volume, center of buoyancy, center-of-mass torque, quaternion rotation, water drag, damping, open-top box flooding, cargo offset, tank wall contacts, buoyant body collisions, debug overlays, and scenarios 1-4.
---

# Skill: Mitai Buoyancy Physics

## Goal
Modify buoyancy behavior while preserving stable, physically legible floating, sinking, tilting, flooding, and wall contact behavior.

## Key Files
- `src/PhysicsSystem.cpp`
- `include/PhysicsSystem.h`
- `include/Entity.h`
- `src/Application.cpp`
- `src/Renderer.cpp`
- `shaders/wave_query.glsl`

## Simulation Model
- Sphere buoyancy uses analytical or specialized submerged volume logic.
- Slab and box buoyancy use point/voxel sampling.
- Buoyant force follows `rho * g * submergedVolume`.
- Torque comes from `cross(centerOfBuoyancy - centerOfMass, buoyantForce)`.
- Open boxes use `floodLevel` to reduce trapped-air displacement and increase sinking tendency.
- Cargo uses mass and local offset to move the effective center of mass.

## Workflow
1. Identify the affected `buoyancyType`: sphere, slab, or open box.
2. Keep force, torque, damping, drag, and integration changes numerically bounded.
3. If changing water sampling, keep renderer water and compute water query logic aligned.
4. Preserve quaternion orientation updates for buoyant objects.
5. Update debug fields in `Entity` when adding new physical quantities.
6. For open-box changes, test ingress, drainage, cargo offset, capsizing, and complete sinking.
7. For contact changes, test tank bottom and all four side walls.

## Constraints
- Substepping & Rigid Stability: Numerical integration of buoyant entities must be calculated using adaptive motion substepping (1 to 8 steps, based on body extent and contact velocity) to prevent velocity and constraint spikes. Normalize orientation quaternions strictly inside the integration loop.
- Hysteresis Flooding & Drainage: Open-top box flooding must utilize top-corner rim submergence triggers with ingress rate hysteresis. Drainage must trigger only via dry timers (e.g. 1.5s delay) to prevent unnatural oscillations.
- Sliding Cargo CoM Coupling: Nested sliding cargo must update its local coordinate displacement along the inner floor and walls of the parent box based on projected gravity, and dynamically alter the parent box's aggregate mass, combined center of mass (worldCoM), and local moment of inertia.
- Physics Grabbing Spring: Interactive mouse grabbing must be calculated as a localized spring-damper joint force (Ks/Kd) with explicit maximum force bounds, rather than direct position teleportation.
- Tank & Entity Collisions: Post-buoyancy contact constraints (e.g., tank boundaries, sphere-sphere, box-box) must be solved via iterative impulse resolution (e.g. 4 iterations) to guarantee stability.
- Do not directly set final orientation as a shortcut for stabilization.
- Avoid unbounded force or torque values; clamp extreme impulses.
- Keep debug overlay data cleared and repopulated per frame.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Test scenarios 1, 2, 3, and 4 from the UI or number keys.
- Use F1 debug overlay to verify buoyancy force, gravity, center lines, wall contacts, and ripple injection.
