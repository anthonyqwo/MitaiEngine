---
name: mitai-collision-physics
description: Use for MitaiEngine collision and rigid-body physics work, including dynamic spheres, sphere-AABB collisions, sphere-sphere response, spatial grid broad phase, collision demo behavior, restitution/friction tuning, substepping, collision count metrics, and resolving tunneling or unstable contacts.
---

# Skill: Mitai Collision Physics

## Goal
Maintain the collision demo and collision primitives while keeping performance and contact response predictable.

## Key Files
- `src/PhysicsSystem.cpp`
- `include/PhysicsSystem.h`
- `include/Collider.h`
- `include/Entity.h`
- `src/Application.cpp`

## Architecture
- Dynamic collision demo objects are named `DynamicSphere`.
- Static objects use AABB bounds and `mass = 0.0f`.
- Broad phase can use exhaustive checks or a 7x7x7 spatial grid.
- `g_collisionChecks` reports collision work for UI comparison.
- Motion uses substeps to reduce tunneling at high speed.

## Workflow
1. Decide whether the change affects broad phase, narrow phase, or impulse response.
2. Preserve `g_collisionChecks` accounting when adding new checks.
3. Rebuild dynamic grid cells during solver passes because collision response moves spheres.
4. Keep sphere radius, scale, and local bounds consistent.
5. Use substepping when changing speed, gravity, or contact stiffness.
6. Test both exhaustive and spatial-grid modes.

## Constraints
- Do not assume all entities are dynamic; static geometry and lights have different flags.
- Do not mutate unrelated scene objects when clearing dynamic spheres.
- Keep broad-phase acceleration optional so it can be compared in the UI.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Open Collision Demo, shoot spheres, compare collision counts with grid on/off.
- Check sphere-wall, sphere-floor, sphere-obstacle, and sphere-sphere behavior.
