# Dispatch to Spec Miner (Materials, Physics, Thermodynamics, Reactions & Explosions)

## Task Description
Mine and document all detailed specifications for Requirements R1 and R2 (ШАГ 2 and ШАГ 3).
Read `ORIGINAL_REQUEST.md` (at `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`).

## Objectives
1. Detail the requirements for 64x64 chunk SoA data layout, cache line alignment (e.g. alignas(64)), cell attributes (material_id, temperature, flags/state, velocity/lifetime, etc.).
2. Detail MaterialRegistry and JSON format for `assets/materials/*.json` (properties: id, name, category, color/rendering, density, state_of_matter [POWDER, LIQUID, GAS, SOLID], thermal conductivity, specific heat, melting_temp, boiling_temp, transition materials, viscosity, dispersion rate, flammable/explosive flags, etc.).
3. Detail movement & falling physics for POWDER and LIQUID (density-based displacement, sliding angle of repose, lateral spreading, empty cell swapping).
4. Detail Thermodynamics (cellular heat diffusion equation, thermal conductivity, phase transitions when temp crosses thresholds, ambient cooling).
5. Detail ReactionSystem and `reactions.json` (reactants, catalyst, temperature requirements, products, energy/heat release/absorption, probabilities, explosion trigger).
6. Detail GAS & FIRE physics (buoyancy upwards, turbulence/random jitter, lifetime/decay, burning neighboring flammable materials).
7. Detail ExplosionSystem (shockwave radius, impulse/velocity, mechanical destruction thresholds, thermal blast, particulate ejecta).
8. Examine any existing material definitions or assets in the repo and specify what need to be created.

## State Folder & Deliverables
- Working directory: `c:\Users\Drilla\Desktop\Sandbox`
- State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_spec_miner_survey_2`
- Output: structured specification and analysis report in `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_spec_miner_survey_2\handoff.md`
- Maintain `progress.md` with timestamps during your work.


## 2026-11-10T18:59:44Z
You are teamwork_spec_miner assigned to Survey Phase 2: Materials, Physics, Thermodynamics, Reactions & Explosions.
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_spec_miner_survey_2
Dispatch file: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_spec_miner_survey_2\DISPATCH.md
Original request: c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md

Follow your instructions and read DISPATCH.md and ORIGINAL_REQUEST.md carefully.
Mine all precise specifications, properties, algorithms, formulas, and schema designs for:
- 64x64 SoA Chunk layout and cache alignment
- MaterialRegistry and assets/materials/*.json schemas
- POWDER and LIQUID movement/displacement physics
- Cellular thermodynamics and phase transitions
- ReactionSystem & reactions.json rules
- GAS, FIRE, and ExplosionSystem
Write your findings and comprehensive specification to c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_spec_miner_survey_2\handoff.md.
Maintain progress.md in your state folder.
When complete, notify parent via send_message with a brief summary and handoff path.
