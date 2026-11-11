## 2026-11-10T19:24:35Z
You are the Physics & Rendering Explorer for Milestone 1.
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_m1_physics_render

Read these documents:
- ORIGINAL_REQUEST.md: c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md
- PROJECT.md: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
- SCOPE.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_m1_world_physics\SCOPE.md

Your task is to investigate and design specifications for:
1. Cellular Automata Physics for POWDER and LIQUID (`src/sim/Physics.hpp` and `.cpp`):
   - Bottom-to-top traversal, alternating horizontal scan direction per row/tick to eliminate directional bias.
   - Parity bit tracking (`currentParity = !currentParity`) so cells updated in this tick are skipped.
   - Powder movement:
     * Check directly below `(x, y+1)`. If density condition met, move/swap.
     * If blocked below, check diagonal down-left `(x-1, y+1)` and down-right `(x+1, y+1)` (randomized or alternating order).
   - Liquid movement:
     * Downward gravity and diagonal sliding like powder.
     * Lateral dispersion: if downward/diagonals blocked, spread left/right up to `dispersion` distance.
     * Cliff drop-off priority: if dispersing laterally and a hole/cliff is encountered, immediately fall down rather than continuing across.
   - Symmetric Mutual Density Displacement:
     * Sand ($1600\text{ kg/m}^3$) sinks in water ($1000\text{ kg/m}^3$).
     * Water ($1000\text{ kg/m}^3$) sinks in oil ($800\text{ kg/m}^3$).
     * Air has density $1.2\text{ kg/m}^3$ (or ~1).
     * Displacement is an atomic swap ($A \leftrightarrow B$) transferring material, temperature, lifetime, and marking parity.
2. WorldRenderer (`src/ui/Renderer.hpp` and `.cpp`):
   - Pixel buffer mapping: map each cell to RGBA32 based on material color + colorVariation.
   - Streaming texture upload with SDL2 (`SDL_CreateTexture`, `SDL_UpdateTexture`/`SDL_LockTexture`).
   - Nearest-neighbor filtering: `SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest)` or `GL_NEAREST`.
3. Interactive Brush (`src/ui/Brush.hpp` and `.cpp`):
   - Mouse input handling: LMB paints currently selected material, RMB paints Air (erases).
   - Mouse wheel changes brush radius (clamped e.g. 1 to 50).
   - Continuous stroke interpolation (Bresenham line algorithm) between previous and current mouse positions to prevent gaps during fast mouse movement.
4. Check existing codebase in `src/` to see what code or CMake structure is present.

Output requirements:
Write your structured findings and recommendations to:
`c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_m1_physics_render\handoff.md`
Send a completion message via `send_message` to your parent orchestrator (conversation ID: ddc9ae7d-022c-473d-9787-f1fcb28efe6f).
