# Dispatch: Milestone 2 Sub-Orchestrator (Thermodynamics, Reactions, Gases & Explosions — ШАГ 3)

## Role & Scope
You are the **Sub-Orchestrator for Milestone 2 (M2 — ШАГ 3)**.
Working directory: `c:\Users\Drilla\Desktop\Sandbox`
State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_m2_thermo_reactions`
Parent conversation ID: f72e393b-a84e-41c4-8a10-e63cc04ac202

## Reference Documents
- `ORIGINAL_REQUEST.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
- `PROJECT.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
- Survey Spec Miner Report (read-only): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_spec_miner_survey_2\handoff.md`
- Milestone 1 Final Handoff (read-only): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_m1_world_physics\handoff.md`

## Milestone 2 Objectives & Deliverables
1. **Thermodynamics & Heat Diffusion (`src/sim/Thermodynamics.hpp`/`.cpp`)**:
   - Discrete 2D heat conduction between adjacent cells (von Neumann 4-neighborhood) using harmonic mean contact conductivity $k_{\text{contact}} = \frac{2 k_1 k_2}{k_1 + k_2 + \epsilon}$.
   - Numerical stability clamping ($\le 0.25 \Delta T$ max transfer per neighbor).
   - Ambient cooling/radiation towards ambient temperature ($20.0^\circ\text{C}$).
2. **Phase Transitions**:
   - Automatic state conversion when cell temperature crosses `meltPoint`, `boilPoint`, or `freezePoint`.
   - Latent heat offset ($\Delta T_{\text{latent}}$) to eliminate oscillatory ping-pong flip-flops between states.
   - Transforming cell material ID to `meltMaterialId`, `boilMaterialId`, or `freezeMaterialId`.
3. **ReactionSystem & reactions.json (`src/sim/ReactionSystem.hpp`/`.cpp`, `assets/reactions.json`)**:
   - Declarative JSON format in `assets/reactions.json` defining chemical and physical reactions.
   - Fast $O(1)$ 2D reaction lookup table `ReactionRule* m_table[MAX_MATERIALS][MAX_MATERIALS]` loaded at startup.
   - Comprehensive reaction rules: Water + Lava -> Stone + Steam; Water + Fire -> Steam; Acid + Stone/Wood/Iron -> Gas/Air (corrosion); Fire + Wood/Gunpowder -> Flame + Ash/Explosion; Lava + Water -> Stone/Glass.
4. **Gas & Fire Cellular Automata (`src/sim/Physics.hpp`/`.cpp`)**:
   - Gas physics: upward buoyancy (negative gravity), Brownian turbulent jitter, lifetime decay back to Air.
   - Fire physics: upward flicker, high temperature ($400-800^\circ\text{C}$), neighbor ignition if $T > \text{ignitionTemp}$, lifetime decay emitting Smoke and Ash.
5. **ExplosionSystem (`src/sim/ExplosionSystem.hpp`/`.cpp`)**:
   - Radial shockwave blast with raycasting (Bresenham/DDA) from explosion center.
   - Attenuation of explosive energy based on material `strength` (modeling obstacle shielding behind stone/iron).
   - Mechanical destruction: converting cells to Air or flying particulate debris.
   - Thermal blast: intense flash heating triggering secondary explosive detonations (TNT/Gunpowder chain reactions).
6. **Integration & Unit Testing**:
   - Integrate with `World`, `Physics`, `main.cpp`, and `CMakeLists.txt`.
   - Implement `tests/test_thermo_reactions.cpp` covering heat conduction, phase transitions, reactions, fire/gas lifetime, and explosions. All tests must pass cleanly.

## Execution Procedure
- Decompose scope or run the Explorer -> Worker -> Reviewer -> Challenger -> Auditor cycle.
- Enforce the mandatory anti-cheating warning to Worker:
  > DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.
- Run 3 Explorers -> 1 Worker -> 2 Reviewers -> 2 Challengers -> 1 Forensic Auditor (`teamwork_auditor`).
- Gate check requires ALL criteria to pass (Auditor verdict must be CLEAN).
- Write `SCOPE.md`, `progress.md`, and `GATE_STATUS.md` in your state folder.
- When Gate passes, report back to parent orchestrator via `send_message`.
