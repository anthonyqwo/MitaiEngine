---
name: mitai-ai-engine
description: Use for MitaiEngine HW9 AI Engine work, including autonomous hunting game behavior, prey and predator agents, finite state machines, visual sensors, steering behaviors, capture scoring, round management, AI scene setup, and no-user-control simulation requirements.
---

# Skill: Mitai AI Engine

## Goal
Implement the HW9 autonomous hunting game requirements while keeping AI behavior explainable for grading.

## Key Files
- `AI_ENGINE_TODO.md`
- `include/Entity.h`
- `include/Application.h`
- `src/Application.cpp`
- `src/Scene.cpp`
- `src/Renderer.cpp`
- Add likely files: `include/AISystem.h`, `src/AISystem.cpp`

## Required Game Rules
- Use a large house with rooms, walls, obstacles, or furniture.
- Spawn 10 green preys and 10 blue preys.
- Green and blue preys have different values.
- Higher-value prey must run faster.
- Add two predator types with different color, shape, or strategy.
- Run predator 1 for a fixed time, record score, reset, then run predator 2.
- Preys evade only when they visually detect predators.
- No user control is required during the simulation.

## Recommended AI Model
Use a finite state machine:
- Prey: `Wander`, `Flee`, optional `Hide`.
- Predator: `Search`, `Chase`, `Capture`, optional `Repath`.

Use visual sensors:
- Range check.
- Field-of-view cone check.
- Line-of-sight blocker test against walls/furniture.

Use steering:
- `Seek` for predator chase.
- `Flee` for prey escape.
- `Wander` for idle movement.
- Path-following when A* is available.

## Predator Strategies
- Predator 1: greedy nearest-prey hunter.
- Predator 2: value-aware hunter using a utility score such as `preyValue / distance` or `preyValue * valueWeight - distance * penalty`.

## Workflow
1. Read `AI_ENGINE_TODO.md` and identify the active phase.
2. Keep AI logic in an `AISystem` or similarly isolated module.
3. Add only lightweight AI fields to `Entity` if needed for rendering/UI/debug.
4. Implement round manager separately from per-agent behavior.
5. Expose debug values in ImGui only after core simulation runs.
6. Keep behavior deterministic enough to compare both predators fairly.

## Constraints
- Do not make predators omniscient; target selection must depend on visual detection or remembered/search state.
- Do not allow user input to control the agents during the scoring run.
- Do not let high-value preys violate the assignment rule: value and speed should be positively related.
- Avoid changing unrelated water/buoyancy systems.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Run predator 1 and predator 2 under the same duration and prey count.
- Confirm scoreboard, timer, captures, and winner are visible.
