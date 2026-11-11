# Scope: Milestone 2 (Thermodynamics, Reactions, Gases & Explosions — ШАГ 3)

## Architecture & Responsibilities
Milestone 2 extends the core simulation with thermal physics, chemical & physical reactions, gaseous & fiery state dynamics, and explosive destruction with shockwave raycasting.

### Features in Scope
1. **Feature 9: Cellular Heat Diffusion (`src/sim/Thermodynamics.hpp`/`.cpp`)**:
   - Discrete 2D heat conduction between adjacent cells (von Neumann 4-neighborhood).
   - Harmonic contact conductivity: $k_{\text{contact}} = \frac{2 k_1 k_2}{k_1 + k_2 + \epsilon}$.
   - Numerical stability clamping ($\le 0.25 \Delta T$ max transfer per neighbor to prevent overshoot / oscillation).
   - Ambient heat radiation/cooling towards $T_{\text{ambient}} = 20.0^\circ\text{C}$.
2. **Feature 10: Material Phase Transitions (`src/sim/Thermodynamics.hpp`/`.cpp`)**:
   - Temperature threshold checks: `meltPoint`, `boilPoint`, `freezePoint`.
   - Latent heat offset ($\Delta T_{\text{latent}}$) to eliminate oscillatory ping-pong flips.
   - Material state transformation: `meltMaterialId`, `boilMaterialId`, `freezeMaterialId`.
3. **Feature 11: ReactionSystem & reactions.json (`src/sim/ReactionSystem.hpp`/`.cpp`, `assets/reactions.json`)**:
   - Declarative JSON format in `assets/reactions.json` defining chemical and physical reactions.
   - Fast $O(1)$ 2D reaction lookup table `ReactionRule* m_table[MAX_MATERIALS][MAX_MATERIALS]` loaded at startup.
   - Symmetric / canonical registration so pair order $(A, B)$ or $(B, A)$ resolves correctly.
   - Comprehensive reaction rules: Water + Lava -> Stone + Steam; Water + Fire -> Steam; Acid + Stone/Wood/Iron -> Gas/Air (corrosion); Fire + Wood/Gunpowder -> Flame + Ash/Explosion; Lava + Water -> Stone/Glass.
4. **Feature 12: Gas Cellular Automata (`src/sim/Physics.hpp`/`.cpp`)**:
   - Upward buoyancy (negative gravity: upward movement preference).
   - Brownian turbulent jitter (horizontal dispersion/drifting).
   - Lifetime decay counter back to Air (`EMPTY_MATERIAL_ID`).
5. **Feature 13: Fire & Combustion (`src/sim/Physics.hpp`/`.cpp`)**:
   - Upward flicker and combustion.
   - High temperature ($400-800^\circ\text{C}$).
   - Neighbor ignition when $T > \text{ignitionTemp}$ and flammability probability.
   - Lifetime decay emitting Smoke and Ash.
6. **Feature 14: ExplosionSystem (`src/sim/ExplosionSystem.hpp`/`.cpp`)**:
   - Radial shockwave blast with raycasting (Bresenham/DDA) from explosion epicenter.
   - Energy attenuation based on material `strength` (shielding behind solid obstacles).
   - Mechanical destruction: converting cells to Air or launching particulate debris.
   - Thermal blast: intense flash heating triggering secondary explosive detonations (TNT/Gunpowder chain reactions).
7. **Integration & Unit Testing**:
   - Hook into `World`, `Physics`, `main.cpp`, `CMakeLists.txt`.
   - Implement `tests/test_thermo_reactions.cpp` covering heat diffusion, phase transitions, reactions, fire/gas lifetime, and explosions.

## Code Layout & File Boundaries
- `assets/reactions.json` (New declarative rules asset)
- `src/sim/Thermodynamics.hpp`, `src/sim/Thermodynamics.cpp` (New heat diffusion & phase transition module)
- `src/sim/ReactionSystem.hpp`, `src/sim/ReactionSystem.cpp` (New O(1) reaction table module)
- `src/sim/ExplosionSystem.hpp`, `src/sim/ExplosionSystem.cpp` (New raycast shockwave & blast module)
- `src/sim/Physics.hpp`, `src/sim/Physics.cpp` (Add Gas & Fire cellular automata)
- `src/sim/World.hpp`, `src/sim/World.cpp` (Integrate thermodynamics, reactions, and explosion triggers)
- `tests/test_thermo_reactions.cpp` (New unit test suite for M2)
- `CMakeLists.txt` & `tests/CMakeLists.txt` (Build targets update)
- `src/main.cpp` (Ensure simulation step ticks thermodynamics & reactions)

## Milestones / Iteration Gate
- Iteration loop: Explorers -> Worker -> Reviewers -> Challengers -> Forensic Auditor.
- Pass criteria: Release build passes, 100% unit tests pass, Reviewers APPROVE, Challengers APPROVE, Auditor CLEAN.
