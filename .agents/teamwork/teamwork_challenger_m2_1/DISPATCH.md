## 2026-11-11T07:31:34Z
You are Challenger 1 for Milestone 2 (Thermodynamics, Reactions, Gases & Explosions — ШАГ 3).
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_challenger_m2_1

MANDATORY FIRST STEP: Read ORIGINAL_REQUEST.md at:
c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md

Also read:
- PROJECT.md: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
- SCOPE.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_m2_thermo_reactions\SCOPE.md
- Worker 1 Handoff: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_worker_m2_1\handoff.md

Your role is to adversarially challenge and stress-test the Thermodynamics and Phase Transition implementations:
1. Write/compile an adversarial stress-test executable or standalone harness (e.g. `tests/test_challenger1_thermo.cpp` or add to test targets).
2. Stress-test:
   - High thermal gradient stability: Place 1500 C Iron adjacent to 0 C Ice and 20 C Air. Run for 500 ticks. Verify zero temperature explosions, zero NaN/Inf, zero negative temperatures, monotonic convergence to equilibrium within CFL limits.
   - Ping-pong resistance: Cycle temperature across the melting point (e.g. oscillating input heat) and verify that latent heat offset prevents single-tick ping-pong state flickering.
   - Chunk boundary heat diffusion: Place hot block at x=63 (chunk boundary) and cold block at x=64 (adjacent chunk). Verify heat conducts across chunk boundary seamlessly and wakes up neighboring chunk.
   - Long-term ambient cooling: Verify that a warm block cools to ambient 20.0 C and eventually snaps to equilibrium so the chunk can go to sleep.
3. Run the tests, evaluate results, and provide a clear empirical verdict: APPROVE or REQUEST_CHANGES.

Write your report to `handoff.md` in your state folder and notify me via `send_message`.
