---
name: mitai-ai-assignment-report
description: Use for preparing the HW9 AI Engine document/report, including explaining two prey AI models, two predator AI models, visual-sensor perception, FSM behavior, steering, A* pathfinding, scoring rules, simulation results, screenshots, diagrams, and grading-aligned writeups.
---

# Skill: Mitai AI Assignment Report

## Goal
Create or update the HW9 report so it clearly explains the AI engine design and supports the grading criteria.

## Required Report Coverage
- Game scenario: house, rooms, walls, obstacles/furniture, preys, predators.
- Basic rules: 20 preys, predator 1 round, reset, predator 2 round, score comparison.
- Prey AI model 1: green prey behavior, value, speed, vision, flee logic.
- Prey AI model 2: blue prey behavior, higher value, higher speed, evasion differences.
- Predator AI model 1: nearest-prey greedy hunter.
- Predator AI model 2: value-aware utility hunter.
- Visual sensor: range, FOV, line-of-sight blocker test.
- FSM: states and transitions for prey/predator.
- Movement: seek, flee, wander, path following.
- Navigation: grid and A* pathfinding.
- Capture and scoring.
- Simulation results and winner.
- Fancy ideas if implemented: debug vision cones, path visualization, deterministic comparison, personality tuning.

## Suggested Structure
1. Objective and assignment interpretation.
2. Game scene and rules.
3. Agent types and parameters.
4. Visual sensor model.
5. Prey AI engines.
6. Predator AI engines.
7. Navigation and movement.
8. Round manager and scoring.
9. Results and discussion.
10. Extra features.

## Diagram Ideas
- House layout grid.
- Prey FSM.
- Predator FSM.
- Visual cone and line-of-sight test.
- A* path example.
- Scoreboard/result screenshot.

## Workflow
1. Read `AI_ENGINE_TODO.md` and current implementation status before drafting.
2. Match report claims to implemented behavior.
3. Use concrete parameter values from code when available.
4. Include screenshots only after the demo renders correctly.
5. Keep grading weights visible in the report narrative: AI design, predator implementation, simulation completion, fancy ideas.

## Constraints
- Do not claim features that are not implemented.
- Do not overemphasize PBR/water technology; this assignment grades AI behavior.
- Keep explanations clear enough for a game programming course evaluation.
