# HW9 AI Engine TODO

This TODO tracks the work needed for "2026 Game Programming, Computer Project #4, AI Engine".

## Assignment Target
Build an autonomous hunting game in a large house with multiple rooms, walls, obstacles/furniture, two prey types, and two predator types. Characters must act by themselves using AI engines and visual-sensor-based information detection.

## Acceptance Checklist
- [ ] Large house scene with rooms, walls, door openings, and obstacles/furniture.
- [ ] 10 green preys and 10 blue preys spawn randomly in valid walkable locations.
- [ ] Green and blue preys have different values.
- [ ] Higher-value prey runs faster.
- [ ] Two predator types render with different colors or shapes.
- [ ] Predator 1 hunts for a fixed gameplay time and records score.
- [ ] Scene resets, prey respawn, predator 2 hunts for the same time.
- [ ] Higher-scoring predator is declared winner.
- [ ] Preys are slower than predators but evade when they visually detect predators.
- [ ] Predators and preys use visual sensors, not omniscient world knowledge.
- [ ] No user control is required during the simulation.
- [ ] AI model design is documented for two prey types and two predator types.

## Implementation Plan

### Phase 1: AI Data Model
- [ ] Add AI role/type metadata for prey and predator entities.
- [ ] Add score value, move speed, max speed, vision range, FOV angle, capture radius, state, and target fields.
- [ ] Decide whether AI state lives directly on `Entity` or in a separate `AIAgent` store.
- [ ] Add clear enum names for role, species, and state.

Suggested states:
- Prey: `Wander`, `Flee`, optional `Hide`.
- Predator: `Search`, `Chase`, `Capture`, optional `Repath`.

### Phase 2: House Scene
- [ ] Add `Application::loadAIHuntingScene`.
- [ ] Create floor, multiple rooms, walls, doors, and obstacle/furniture cube entities.
- [ ] Mark walls/furniture as AI blockers and static collision obstacles.
- [ ] Add random spawn points or random walkable-cell spawning.
- [ ] Add ImGui scene selector entry for AI Hunting Demo.

### Phase 3: Navigation Grid
- [ ] Build a 2D grid over the house floor.
- [ ] Mark blocked cells from wall/furniture AABBs.
- [ ] Implement world-to-cell and cell-to-world conversion.
- [ ] Add random valid-cell sampling for prey spawn.
- [ ] Add debug drawing for blocked cells or nav grid when enabled.

### Phase 4: Visual Sensor
- [ ] Implement range check.
- [ ] Implement FOV cone check using forward direction and dot product.
- [ ] Implement line-of-sight ray test against wall/furniture blockers.
- [ ] Return visible prey targets for predators and visible predator threats for preys.
- [ ] Add optional debug vision cones and sight lines.

### Phase 5: Steering Behaviors
- [ ] Implement `Seek` for chasing target positions.
- [ ] Implement `Flee` for escaping visible predators.
- [ ] Implement `Wander` for idle movement.
- [ ] Implement simple obstacle avoidance or path-following.
- [ ] Clamp acceleration/speed to avoid jitter.

### Phase 6: Pathfinding
- [ ] Implement A* over the navigation grid.
- [ ] Use A* when predators need to reach prey across rooms.
- [ ] Use A* or cell scoring for prey escape destinations.
- [ ] Repath when target moves, line-of-sight changes, or path is blocked.
- [ ] Add path debug lines for selected agents.

### Phase 7: AI Behaviors
- [ ] Green prey: lower value, slower speed, wander and flee.
- [ ] Blue prey: higher value, faster speed, possibly longer vision or sharper evasion.
- [ ] Predator 1: nearest-prey greedy hunter.
- [ ] Predator 2: value-aware hunter using utility score such as `value / distance`.
- [ ] Ensure predators are faster than preys.
- [ ] Ensure preys only flee after visual detection.

### Phase 8: Round and Score Manager
- [ ] Add simulation timer.
- [ ] Spawn preys, run predator 1, record score.
- [ ] Clear active prey/predator state.
- [ ] Respawn preys, run predator 2, record score.
- [ ] Declare winner.
- [ ] Add score/time/result UI overlay.

### Phase 9: Capture Logic
- [ ] Detect capture by distance or collision radius.
- [ ] Add prey value to predator score.
- [ ] Remove or hide captured prey.
- [ ] Log capture events for debug/demo clarity.

### Phase 10: Runtime Debugging and Polish
- [ ] Add AI debug overlay toggle.
- [ ] Draw vision cones, current target lines, path lines, and state labels.
- [ ] Add ImGui controls for round duration, sensor range, FOV, speeds, and reset.
- [ ] Add deterministic seed option for reproducible comparisons.
- [ ] Build and test with `cmake --build build_dist --config MinSizeRel`.

### Phase 11: Assignment Document
- [ ] Explain the game scenario and rules.
- [ ] Describe the two prey AI models.
- [ ] Describe the two predator AI models.
- [ ] Explain visual sensor design: range, FOV, line of sight.
- [ ] Explain FSM and steering behaviors.
- [ ] Explain A* pathfinding and obstacle handling.
- [ ] Include screenshots or diagrams of house scene, vision cones, paths, and scoreboard.
- [ ] Compare predator scores and discuss why one strategy wins.

## Suggested File Additions
- `include/AISystem.h`
- `src/AISystem.cpp`
- `include/NavigationGrid.h`
- `src/NavigationGrid.cpp`

## Existing Files Likely To Change
- `include/Entity.h`
- `include/Application.h`
- `src/Application.cpp`
- `include/Renderer.h`
- `src/Renderer.cpp`
- `CMakeLists.txt`
- `CMakeLists_dist.txt`

## Relevant Skills
- `mitai-ai-engine`
- `mitai-ai-navigation`
- `mitai-ai-assignment-report`
- `mitai-runtime-debugging`
- `mitai-collision-physics`
- `mitai-rendering-pipeline`
- `mitai-build-release`
