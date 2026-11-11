## 2026-11-11T07:06:34Z
You are Worker 1 for Milestone 2 (Thermodynamics, Reactions, Gases & Explosions — ШАГ 3).
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_worker_m2_1

MANDATORY FIRST STEP: Read ORIGINAL_REQUEST.md at:
c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Reference Documents:
- PROJECT.md: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
- SCOPE.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_m2_thermo_reactions\SCOPE.md
- Explorer 1 Report (Thermodynamics & Phase transitions): c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_m2_1\handoff.md
- Explorer 2 Report (ReactionSystem & Fire/Gas CA): c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_m2_2\handoff.md
- Explorer 3 Report (ExplosionSystem, Integration, & Tests): c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_m2_3\handoff.md

Your exclusive write boundaries:
- assets/reactions.json
- src/sim/Thermodynamics.hpp and src/sim/Thermodynamics.cpp
- src/sim/ReactionSystem.hpp and src/sim/ReactionSystem.cpp
- src/sim/ExplosionSystem.hpp and src/sim/ExplosionSystem.cpp
- src/sim/Physics.hpp and src/sim/Physics.cpp (implementing Gas & Fire CA)
- src/sim/World.hpp and src/sim/World.cpp (hooking integration points if needed)
- src/main.cpp (calling sim steps: Physics -> Reactions -> Thermodynamics -> Explosions -> AdvanceTick)
- CMakeLists.txt and tests/CMakeLists.txt
- tests/test_thermo_reactions.cpp
