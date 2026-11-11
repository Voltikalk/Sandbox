# Dispatch: Milestone 1 Sub-Orchestrator (World, Chunks, MaterialRegistry & Basic Physics/Rendering)

## Role & Scope
You are the **Sub-Orchestrator for Milestone 1 (M1 - ШАГ 2)**.
Working directory: `c:\Users\Drilla\Desktop\Sandbox`
State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_m1_world_physics`
Parent conversation ID: f72e393b-a84e-41c4-8a10-e63cc04ac202

## Reference Documents
- `ORIGINAL_REQUEST.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
- `PROJECT.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
- Survey Spec Miner Report (read-only): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_spec_miner_survey_2\handoff.md`
- Survey Explorer Report (read-only): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_survey_1\handoff.md`

## Milestone 1 Objectives & Deliverables
1. **64x64 SoA Chunk Data Layout**:
   - `alignas(64)` cache-line aligned Structure of Arrays: `material[4096]`, `temperature[4096]`, `flags[4096]`, `lifetime[4096]`.
   - Indexing `(ly << 6) | lx`. Bitwise parity flag (`FLAG_PARITY = 0x01`) and bottom-to-top traversal to prevent double-updates.
2. **MaterialRegistry & JSON Assets**:
   - `MaterialRegistry` singleton / manager loading all `assets/materials/*.json`.
   - Complete 18 core material JSON files (`air.json`, `stone.json`, `dirt.json`, `sand.json`, `water.json`, `ice.json`, `steam.json`, `oil.json`, `acid.json`, `lava.json`, `fire.json`, `smoke.json`, `wood.json`, `iron.json`, `gunpowder.json`, `tnt.json`, `glass.json`, `ash.json`).
3. **Physics of POWDER & LIQUID**:
   - Powder vertical fall and angle-of-repose diagonal sliding.
   - Liquid vertical fall, diagonal sliding, and lateral flow up to `dispersion` distance with cliff drop-off priority.
   - Symmetric density-based mutual displacement ($A \leftrightarrow B$ swap) for natural sinking/buoyancy.
4. **Streaming Texture Rendering**:
   - `WorldRenderer` rendering the pixel world via streaming texture with `GL_NEAREST` filtering, without graphical artifacts.
5. **Interactive Brush Tool**:
   - Brush supporting LMB painting with selected material, RMB erasing (drawing Air), mouse wheel changing brush radius, and continuous stroke interpolation.
6. **Integration & Build**:
   - Hook into `src/main.cpp` loop and `CMakeLists.txt`. Ensure project compiles cleanly in Release and Debug modes.

## Sub-Orchestrator Execution Procedure
- Decompose scope or run the Explorer -> Worker -> Reviewer -> Challenger -> Auditor cycle.
- Remember: 3 Explorers, 1 Worker, 2 Reviewers, 2 Challengers, 1 Forensic Auditor (`teamwork_auditor`).
- Enforce the mandatory anti-cheating warning to Worker.
- Auditor verdict is a hard binary veto.
- Write `SCOPE.md`, `progress.md`, `GATE_STATUS.md` in your state folder.
- When Gate passes, report back to parent orchestrator via `send_message`.


## 2026-11-11T05:47:26Z
**Context**: Server reboot recovery
**Content**: The server has been restarted and network connectivity is fully restored. Please recover your state from your state folder c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_m1_world_physics, check on any subagents or restart them, and continue Milestone 1 execution per SCOPE.md and PROJECT.md.
**Action**: Resume Milestone 1 execution and report your status.
