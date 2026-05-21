---
name: mitai-ai-navigation
description: Use for MitaiEngine AI navigation work, including house navigation grids, obstacle/furniture blockers, random valid spawning, A* pathfinding, line-of-sight ray tests, FOV checks, path following, obstacle avoidance, and debug drawing of AI paths or visual sensors.
---

# Skill: Mitai AI Navigation

## Goal
Provide navigation and perception support for the HW9 hunting game so agents can move through rooms without walking through walls and can detect targets by visual sensors.

## Key Files
- `AI_ENGINE_TODO.md`
- `include/Entity.h`
- `include/Collider.h`
- `src/Application.cpp`
- `src/Renderer.cpp`
- Add likely files: `include/NavigationGrid.h`, `src/NavigationGrid.cpp`

## Navigation Grid
Represent the house floor as a 2D grid:
- Each cell has walkable/blocked state.
- Walls and furniture mark blocked cells.
- Door openings remain walkable.
- World positions map to grid cells and grid cells map to world centers.

## A* Pathfinding
Use A* over the grid:
- Neighbors: 4-way for simplicity or 8-way with corner-cut prevention.
- Heuristic: Manhattan for 4-way, octile/Euclidean for 8-way.
- Output: list of world-space waypoints.
- Repath when target changes significantly or path becomes stale.

## Visual Sensor
Use a three-stage test:
1. Distance within vision range.
2. Target inside FOV cone using dot product.
3. Line segment from eye to target is not blocked by wall/furniture AABBs.

## Workflow
1. Build the blocker list from house walls/furniture entities.
2. Generate or update the grid after the house scene is loaded.
3. Add random valid spawn sampling for preys and predator starts.
4. Implement pathfinding independent of rendering.
5. Add optional debug rendering for grid, path, FOV, and line of sight.
6. Keep navigation in XZ plane unless the assignment needs vertical movement.

## Constraints
- Do not use visual detection that ignores walls.
- Do not allow agents to spawn inside blocked cells.
- Keep pathfinding deterministic for fair predator comparison.
- Avoid navmesh complexity unless grid navigation becomes insufficient.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Spawn agents in multiple rooms and confirm they can navigate through doors.
- Confirm line-of-sight fails through walls and succeeds through door openings.
