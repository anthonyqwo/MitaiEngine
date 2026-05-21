---
name: mitai-runtime-debugging
description: Use for MitaiEngine runtime tooling, ImGui editor, debug overlays, profiling, keyboard shortcuts, scene hierarchy, inspector controls, water debug modes, G-Buffer visualization, buoyancy debug lines, GPU timer queries, and interactive mouse grabbing.
---

# Skill: Mitai Runtime Debugging

## Goal
Add or refine runtime controls and diagnostics so rendering and physics behavior can be inspected without destabilizing the simulation.

## Key Files
- `src/Application.cpp`
- `src/Renderer.cpp`
- `include/Application.h`
- `include/Entity.h`
- `include/Renderer.h`

## Existing Controls
- Tab: toggle mouse look/cursor.
- F1: buoyancy debug overlay.
- F2: GPU profiling overlay.
- F3: G-Buffer visualization.
- F4: water waves toggle.
- F5: water debug mode cycle.
- 1-4: buoyancy scenarios when cursor is available.
- ImGui: scene hierarchy, inspector, engine controls, collision controls, water tuning.

## Workflow
1. Add state to `Application.h` only when it must persist across frames.
2. Add key toggles with press/release edge detection to avoid repeated toggles.
3. Add ImGui controls near related systems.
4. For debug rendering, store per-frame debug data on `Entity` and draw it in `Renderer`.
5. For GPU timing, wrap the smallest meaningful pass with timer queries and show results in the F2 overlay.
6. Preserve mouse grabbing as a physics-driven interaction.

## Constraints
- ImGui Slider Precision Hint: Always instruct users to utilize Ctrl + Click on ImGui sliders to input precise decimal values. This prevents massive jumps in float values that can break fluid simulation or rigid physics stability.
- Document and Style Integrity: All diagnostic tools, comments, or documentation must follow a highly professional tone. Emojis are strictly forbidden in any source code comments, UI labels, README.md, PROGRESS.md, or generated files.
- Do not let UI controls mutate invalid entity indices.
- Avoid expensive debug drawing unless the related overlay is enabled.
- Keep debug visualization optional and off by default unless already enabled by the current scene.
- Do not reuse keyboard shortcuts without updating the instructions panel.

## Validation
- Build with `cmake --build build_dist --config MinSizeRel`.
- Toggle F1-F5 and verify UI text matches behavior.
- Select entities in the hierarchy and adjust material/physics values without crashes.
