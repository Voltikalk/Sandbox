# BRIEFING — 2026-11-11T07:03:00Z

## Mission
Investigate and design ReactionSystem (reactions.json format, O(1) 2D symmetric lookup table, comprehensive rules) and Gas/Fire Cellular Automata physics (buoyancy, Brownian jitter, lifetime decay, flicker, ignition, smoke/ash emission, parity artifact prevention) for Milestone 2.

## 🔒 My Identity
- Archetype: explorer
- Roles: investigation, design, synthesis
- Working directory: c:\Users\Drilla\Desktop\Sandbox
- State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_m2_2
- Original parent: 452cac6c-0128-4a9c-9ecf-dde17c8213e1
- Milestone: Milestone 2 (Thermodynamics, Reactions, Gases & Explosions)

## 🔒 Key Constraints
- Read-only investigation — do NOT implement source code
- Produce detailed, actionable report with code designs and recommendations in handoff.md in state folder
- Use send_message to report back to parent

## Current Parent
- Conversation ID: 452cac6c-0128-4a9c-9ecf-dde17c8213e1
- Updated: 2026-11-11T07:03:00Z

## Investigation State
- **Explored paths**:
  - `ORIGINAL_REQUEST.md`, `PROJECT.md`, `SCOPE.md`, M1 handoff
  - `src/sim/Chunk.hpp`, `src/sim/Chunk.cpp`, `src/sim/World.hpp`, `src/sim/World.cpp`
  - `src/sim/Material.hpp`, `src/sim/MaterialRegistry.hpp`, `src/sim/MaterialRegistry.cpp`
  - `src/sim/Physics.hpp`, `src/sim/Physics.cpp`
  - All 18 `assets/materials/*.json` definitions
- **Key findings**:
  - `ReactionSystem` requires a symmetric 2D table `m_table[MAX_MATERIALS][MAX_MATERIALS]` where `m_table[A][B]` and `m_table[B][A]` are registered with inverse product assignments so `rx->productA` always transforms the querying cell.
  - Storing flat `ReactionResult m_table[256][256]` with `hasReaction` bool gives $O(1)$ zero-allocation lookups without pointer invalidation bugs.
  - Gas upward movement requires negative gravity, Brownian jitter (randomized up/diag-up/lateral), ceiling venting priority (inverse of liquid cliff drop-off), and lifetime decay.
  - Fire requires high temperature maintenance (400-800°C), neighbor ignition based on flammability and ignitionTemp, upward flicker, and decay into Smoke/Ash/Air.
  - Parity protection: `World::SwapCells` already stamps `m_currentParity` on both cells, preventing rocket gas multi-stepping during bottom-to-top traversal. Newly placed/converted cells from reactions must also receive `currentParity` to prevent intra-tick cascades.
- **Unexplored areas**: None within Explorer 2 scope. Ready for handoff synthesis.

## Key Decisions Made
- Reaction table schema: flat 256x256 struct array with `hasReaction` flag for zero pointer chasing, zero allocation risk, and $O(1)$ fast lookup.
- Symmetric registration contract: For rule $A + B \to P_A + P_B$, table entry $[A][B]$ has `productA = PA, productB = PB`, and table entry $[B][A]$ has `productA = PB, productB = PA`.
- Gas dispersion logic: Upward raycast -> diagonal up -> horizontal dispersion with chimney/vent upward opening detection.
- Fire lifecycle: Core heat boost (400-800°C) -> neighbor ignition check -> upward flicker -> decay emitting Smoke (65%), Ash (15%), or Air (20%).

## Artifact Index
- DISPATCH.md — incoming instructions
- progress.md — liveness heartbeat
- BRIEFING.md — persistent memory
- handoff.md — final handoff report
