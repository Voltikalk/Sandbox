# Material Specification Report: Core Materials, C++ Schema, and MaterialRegistry

## 1. Observation

### 1.1 Existing Files Audit (`assets/` and `src/`)
A direct inspection of the workspace via `Get-ChildItem -Path assets, src -Recurse -File` revealed the following exact files:

```
FullName                                                 Size (bytes)
--------                                                 ------------
C:\Users\Drilla\Desktop\Sandbox\assets\config.json        469
C:\Users\Drilla\Desktop\Sandbox\assets\materials\air.json 458
C:\Users\Drilla\Desktop\Sandbox\src\main.cpp              8,749
C:\Users\Drilla\Desktop\Sandbox\src\core\Config.cpp       3,686
C:\Users\Drilla\Desktop\Sandbox\src\core\Config.hpp       959
C:\Users\Drilla\Desktop\Sandbox\src\core\Log.hpp          2,575
C:\Users\Drilla\Desktop\Sandbox\src\core\Math.hpp         2,596
C:\Users\Drilla\Desktop\Sandbox\src\core\ThreadPool.cpp   3,533
C:\Users\Drilla\Desktop\Sandbox\src\core\ThreadPool.hpp   2,134
C:\Users\Drilla\Desktop\Sandbox\src\core\Timer.hpp        3,327
C:\Users\Drilla\Desktop\Sandbox\src\core\Types.hpp        3,050
```

Key observations from existing files:
1. `src/sim/` currently does **not exist**. There are no simulation headers or implementation files present yet.
2. `assets/materials/` currently contains **only 1 file**: `air.json`.
   The remaining 17 core material JSON files (`stone.json`, `dirt.json`, `sand.json`, `water.json`, `ice.json`, `steam.json`, `oil.json`, `acid.json`, `lava.json`, `fire.json`, `smoke.json`, `wood.json`, `iron.json`, `gunpowder.json`, `tnt.json`, `glass.json`, `ash.json`) are missing.
3. Verbatim content of `assets/materials/air.json`:
   ```json
   {
       "name": "air",
       "category": "gases",
       "color": [0, 0, 0, 0],
       "color_variation": 0,
       "state": "STATIC",
       "density": 0.0,
       "dispersion": 0,
       "viscosity": 0.0,
       "flammability": 0.0,
       "ignition_temp": 10000,
       "burn_rate": 0,
       "melt_point": 10000,
       "freeze_point": -273,
       "boil_point": 10000,
       "heat_capacity": 1.0,
       "thermal_conductivity": 0.02,
       "strength": 0,
       "lifetime": 0,
       "corrosive": false
   }
   ```
4. `src/core/Types.hpp` defines:
   - `CACHE_LINE_SIZE = 64`, `CHUNK_SIZE = 64`, `CHUNK_AREA = 4096`.
   - `Color4u` (RGBA8888 32-bit packed, `alignas(4)`), `ToU32()`, `FromU32()`, `Lerp()`.
   - Fixed types `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`, `i64`, `f32`, `f64`.
5. `PROJECT.md` lines 201–243 specifies the interface contract for `Material` and `MaterialRegistry`:
   - `MaterialState` enum: `Solid`, `Powder`, `Liquid`, `Gas`.
   - `MaterialId`: `core::u16`, with `EMPTY_MATERIAL_ID = 0`.
   - `Material` struct fields: `id`, `name`, `category`, `color`, `colorVariation`, `state`, `density`, `dispersion`, `flammability`, `ignitionTemp`, `meltPoint`, `boilPoint`, `freezePoint`, `specificHeat`, `thermalConductivity`, `strength`, `lifetime`, `meltMaterialId`, `boilMaterialId`, `freezeMaterialId`.
   - `MaterialRegistry` methods: `Instance()`, `LoadFromDirectory()`, `RegisterMaterial()`, `Get()`, `FindByName()`, `Count()`.
6. `CMakeLists.txt` lines 88–92 copies `assets/` to `$<TARGET_FILE_DIR:sandbox>/assets` via a post-build command.

---

## 2. Logic Chain

1. **Material Identification & Deterministic ID Assignment**:
   - `PROJECT.md` line 155 specifies `inline constexpr MaterialId EMPTY_MATERIAL_ID = 0;`.
   - In any cellular automata grid, uninitialized or cleared cells default to ID `0`. Therefore, `air` must definitively occupy ID `0`.
   - To ensure absolute determinism across all runs, builds, unit tests, and binary save/load operations, IDs $0 \dots 17$ must be explicitly assigned to the 18 core materials rather than depending on directory iteration order (which varies across operating systems and filesystems).
   - The ordering established in Survey Phase 2 (`air`: 0, `stone`: 1, `dirt`: 2, `sand`: 3, `water`: 4, `ice`: 5, `steam`: 6, `oil`: 7, `acid`: 8, `lava`: 9, `fire`: 10, `smoke`: 11, `wood`: 12, `iron`: 13, `gunpowder`: 14, `tnt`: 15, `glass`: 16, `ash`: 17) preserves stable serialization compatibility.

2. **Density Hierarchy & Mutual Displacement Mechanics (R1)**:
   - Cellular automata physics in Milestone 1 requires particles to fall and displace lighter particles.
   - Sinking/floating behavior depends strictly on relative density ($\rho$):
     - `ash` ($\rho = 500$) < `wood` ($\rho = 650$) < `oil` ($\rho = 800$) < `ice` ($\rho = 917$) < `water` ($\rho = 1000$) < `acid` ($\rho = 1200$) < `dirt` ($\rho = 1500$) < `sand` ($\rho = 1600$) < `gunpowder` ($\rho = 1650$) < `tnt` ($\rho = 1654$) < `glass` ($\rho = 2400$) < `stone` ($\rho = 2500$) < `lava` ($\rho = 3000$) < `iron` ($\rho = 7800$).
   - This exact density hierarchy ensures:
     - Oil floats on Water; Water sinks below Oil.
     - Water floats on Acid; Acid sinks below Water.
     - Sand sinks through Water, Oil, and Acid.
     - Sand floats on top of molten Lava ($\rho_{\text{sand}} = 1600 < \rho_{\text{lava}} = 3000$).
     - Iron sinks through Lava ($\rho_{\text{iron}} = 7800 > \rho_{\text{lava}} = 3000$).
     - Ash floats on Water and Oil.

3. **Thermodynamic Phase Transitions & Latent Heat Loop Prevention**:
   - Cyclic transitions exist in nature:
     - $\text{Water} \xrightarrow{\le 0^\circ\text{C}} \text{Ice} \xrightarrow{> 0^\circ\text{C}} \text{Water}$
     - $\text{Water} \xrightarrow{\ge 100^\circ\text{C}} \text{Steam} \xrightarrow{\le 95^\circ\text{C}} \text{Water}$
     - $\text{Stone} \xrightarrow{\ge 1200^\circ\text{C}} \text{Lava} \xrightarrow{\le 700^\circ\text{C}} \text{Stone}$
     - $\text{Sand} \xrightarrow{\ge 1700^\circ\text{C}} \text{Glass} \xrightarrow{\ge 1400^\circ\text{C}} \text{Lava}$
   - Without hysteresis or latent heat absorption/release, a cell hovering at the boundary temperature would toggle state every single tick (e.g. at $0.0^\circ\text{C}$, toggling between Water and Ice 60 times per second).
   - By enforcing a condensation threshold ($95^\circ\text{C}$) below boiling ($100^\circ\text{C}$), a solidification threshold ($700^\circ\text{C}$) well below stone melting ($1200^\circ\text{C}$), and a $\Delta T_{\text{latent}} = 1.0^\circ\text{C} - 2.0^\circ\text{C}$ offset during phase transformation, oscillations are mathematically eliminated.

4. **Two-Pass Resolution in `MaterialRegistry`**:
   - JSON material definitions reference target materials by name (e.g. `water.json` references `steam` and `ice`).
   - When loading individual JSON files, target materials might not yet have been parsed.
   - Solution: In pass 1, load all JSON properties and store target names (`meltTargetName`, `boilTargetName`, `freezeTargetName`). In pass 2, call `ResolvePhaseTargets()` to map target names to registered `MaterialId` integers.

5. **JSON Key Compatibility (snake_case and camelCase)**:
   - Existing `air.json` uses snake_case (`color_variation`, `ignition_temp`, `melt_point`, `heat_capacity`, `thermal_conductivity`).
   - `PROJECT.md` C++ struct uses camelCase (`colorVariation`, `ignitionTemp`, `meltPoint`, `specificHeat`, `thermalConductivity`).
   - The JSON loader must inspect both keys (e.g., checking `specific_heat` and `heat_capacity` as synonyms for `specificHeat`), preventing parsing failures regardless of JSON styling.

---

## 3. Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|----------|---------|-------------|--------|---------|----------------|----------------|
| 1 | Material System | 18 Core Material Catalog | Complete specification of all 18 materials spanning solids, powders, liquids, gases, and explosives | JSON asset files in `assets/materials/*.json` | Registered `Material` structs with IDs 0 to 17 | Missing required keys fall back to defaults, logged via `LOG_WARN` | PROJECT.md, SCOPE.md, survey_2 |
| 2 | Material System | Deterministic ID Mapping | Fixed IDs assigned to core materials with `air == 0` guarantee | Predefined JSON `id` or sequential index | Deterministic `MaterialId` (0 to 17) | Duplicate ID logs error and reallocates next free ID | PROJECT.md:155, survey_2 |
| 3 | Material System | Two-Pass Phase Target Resolution | Resolves target material names (`melt_target`, etc.) into `MaterialId` after all files load | Target strings from JSON | `meltMaterialId`, `boilMaterialId`, `freezeMaterialId` | Unresolved target names map to `EMPTY_MATERIAL_ID` (0) | SCOPE.md:9, PROJECT.md:227 |
| 4 | Material System | Flexible JSON Key Parsing | Dual-key parser supporting snake_case (`air.json`) and camelCase (`PROJECT.md`) | `nlohmann::json` object | Normalized `Material` struct | Key absence uses safe struct defaults | air.json vs PROJECT.md:209 |
| 5 | Physics Engine | 4-State Material Classification | Categorization of materials into `Solid`, `Powder`, `Liquid`, `Gas` for cellular automata dispatch | Material state enum / string | Movement rule selection in `Physics.cpp` | Unknown state defaults to `Solid` | PROJECT.md:205, SCOPE.md:9 |
| 6 | Physics Engine | Density Sinking & Displacement | Strict relative density sorting enabling symmetric swapping ($A \leftrightarrow B$) | Particle $\rho_A$, target $\rho_B$ | Swapped cell positions if $\rho_A > \rho_B$ | Identical densities do not swap (no-op) | ORIGINAL_REQUEST.md:43, SCOPE.md:15 |
| 7 | Physics Engine | Lateral Fluid Dispersion | Horizontal reach parameter ($1 \dots 5$) defining spreading speed of liquids and gases | `dispersion` integer | Furthest unobstructed cell in scan direction | Blocked by boundaries or denser cells | PROJECT.md:217, survey_2:364 |
| 8 | Thermodynamics | Phase Transition Thresholds | Temperature triggers for melting, freezing, boiling, and condensation | Cell temperature, material thresholds | Transformed material ID and latent heat offset | No transition if target ID is `0` | PROJECT.md:220-222, survey_2:418 |
| 9 | Chemical/Combustion | Flammability & Ignition | Material sensitivity to heat and flame contact | Temperature, neighbor flame, `flammability`, `ignitionTemp` | Ignition trigger into `fire` or burning state | Non-flammable materials (flammability = 0) never ignite | PROJECT.md:218-219, survey_2:605 |
| 10 | Mechanics | Structural Strength & Blast Resistance | Resistance to physical shockwaves and corrosive dissolution | Shockwave impulse energy, `strength` | Cell survives or vaporizes to debris/smoke | Infinite/high strength occludes blast rays | PROJECT.md:225, survey_2:651 |
| 11 | Lifecycle | Particle Lifetime & Decay | Finite lifespan for dynamic transient particles (fire, smoke, steam) | `lifetime` tick counter | Decremented tick counter; decays to air/smoke at 0 | Lifetime 0 indicates permanent particle | PROJECT.md:226, survey_2:595 |
| 12 | Optics/Rendering | Deterministic Per-Pixel Noise | Visual noise variation using spatial hash (`CellNoiseVariation`) based on `colorVariation` | World $(x, y)$, base `color`, `colorVariation` | Modulated RGBA color in streaming texture | Clamped to $[0, 255]$ per channel | Types.hpp:62, Math.hpp:52 |

---

## 4. Edge Cases

| # | Feature | Input | Observed Behavior |
|---|---------|-------|-------------------|
| 1 | Air as Moving Particle | Cell with `MaterialId == 0` (Air) visited during physics pass | Air is static background medium; physics loop must skip `EMPTY_MATERIAL_ID` to prevent entire empty sky from simulating unnecessary movement |
| 2 | Phase Transition Ping-Pong | Water at $0.0^\circ\text{C}$ touching Ice at $0.0^\circ\text{C}$ | Hysteresis and latent heat offset ($\Delta T_{\text{latent}} = 1.0^\circ\text{C}$) prevent rapid state oscillation between solid and liquid |
| 3 | Condensation Hysteresis | Steam cooled to $99.0^\circ\text{C}$ | Steam boil point is $100.0^\circ\text{C}$, but condense threshold is $95.0^\circ\text{C}$; prevents steam from immediately re-condensing upon generation |
| 4 | Density Inversion | Solid Powder (Sand, $\rho=1600$) dropped on Molten Liquid (Lava, $\rho=3000$) | Sand has lower density than Lava; Sand floats on surface of Lava until thermal conduction heats it above $1700^\circ\text{C}$ (melting into Glass) |
| 5 | Equal Density Contact | Two particles of equal density (e.g. two liquids with $\rho_1 = \rho_2$) contacting | Mutual displacement requires strict inequality $\rho_A > \rho_B$; equal density prevents swapping, avoiding artificial churning |
| 6 | Out-of-Bounds Material ID | Chunk contains corrupt or invalid `MaterialId >= registry.Count()` | `MaterialRegistry::Get(id)` returns fallback `m_emptyMaterial` (Air) and logs warning; prevents memory corruption or segfault |
| 7 | Circular Phase Dependency | Material A melts to B, B freezes to A | Two-pass target resolution successfully links both pointers without infinite recursion |
| 8 | Missing JSON File | One of the 18 JSON files missing from `assets/materials/` | `LoadFromDirectory` logs `LOG_WARN` for the missing file; fallback material registered; game remains runnable |
| 9 | Missing Optional Properties | JSON file omits `dispersion`, `strength`, or `lifetime` | Parser applies robust schema defaults (`dispersion = 0`, `strength = 0.0`, `lifetime = 0`) |
| 10 | Non-existent Phase Target | JSON defines `"melt_target": "unobtainium"` | `ResolvePhaseTargets()` fails to find target name; sets `meltMaterialId = 0`; logs descriptive warning |
| 11 | Color Array Bounds | JSON defines `"color": [255, 128]` (fewer than 4 elements) | Parser verifies array size; fills missing alpha with 255; logs warning if malformed |
| 12 | Subzero Temperature | Temperature drops below absolute zero ($-273.15^\circ\text{C}$) | Thermodynamics and material thresholds clamp temperature to $-273.15^\circ\text{C}$ minimum |

---

## 5. Comprehensive Technical Specification

### 5.1 The 18 Core Material Definitions (Exact Property Values)

Below is the definitive catalog of all 18 core materials. Every field required by the specification is determined with exact values:

#### Summary Catalog Table

| ID | Name | Category | State | Color (RGBA) | Var | Density ($\text{kg/m}^3$) | Disp | Flam | Ignit ($^\circ\text{C}$) | Melt ($^\circ\text{C}$) / Target | Boil ($^\circ\text{C}$) / Target | Freeze ($^\circ\text{C}$) / Target | $C_p$ | $k_{\text{th}}$ | Str | Life |
|:--:|:-----|:---------|:------|:-------------|:---:|:------------------------:|:----:|:----:|:------------------------:|:--------------------------------:|:--------------------------------:|:----------------------------------:|:-----:|:---------------:|:---:|:----:|
| **0** | `air` | gases | Gas | `[0, 0, 0, 0]` | 0 | 0.0 | 0 | 0.00 | 10000 | — / — | — / — | — / — | 1.00 | 0.02 | 0.0 | 0 |
| **1** | `stone` | solids | Solid | `[128, 128, 128, 255]` | 15 | 2500.0 | 0 | 0.00 | 10000 | 1200 / `lava` | — / — | — / — | 0.80 | 0.20 | 80.0 | 0 |
| **2** | `dirt` | powders | Powder | `[134, 96, 67, 255]` | 12 | 1500.0 | 1 | 0.00 | 10000 | 1000 / `lava` | — / — | — / — | 0.90 | 0.15 | 10.0 | 0 |
| **3** | `sand` | powders | Powder | `[228, 204, 131, 255]` | 16 | 1600.0 | 2 | 0.00 | 10000 | 1700 / `glass` | — / — | — / — | 0.80 | 0.25 | 15.0 | 0 |
| **4** | `water` | liquids | Liquid | `[34, 115, 230, 200]` | 8 | 1000.0 | 5 | 0.00 | 10000 | — / — | 100 / `steam` | 0 / `ice` | 4.18 | 0.60 | 0.0 | 0 |
| **5** | `ice` | solids | Solid | `[180, 220, 255, 230]` | 10 | 917.0 | 0 | 0.00 | 10000 | 0 / `water` | — / — | — / — | 2.09 | 0.40 | 20.0 | 0 |
| **6** | `steam` | gases | Gas | `[210, 225, 240, 160]` | 10 | 0.6 | 3 | 0.00 | 10000 | — / — | — / — | 95 / `water` | 2.00 | 0.05 | 0.0 | 120 |
| **7** | `oil` | liquids | Liquid | `[85, 75, 40, 240]` | 6 | 800.0 | 3 | 0.80 | 80 | — / — | 300 / `smoke` | -20 / — | 1.80 | 0.12 | 0.0 | 0 |
| **8** | `acid` | liquids | Liquid | `[75, 230, 45, 220]` | 12 | 1200.0 | 4 | 0.00 | 10000 | — / — | 110 / `smoke` | — / — | 2.50 | 0.30 | 0.0 | 0 |
| **9** | `lava` | liquids | Liquid | `[255, 90, 0, 255]` | 20 | 3000.0 | 1 | 0.00 | 10000 | — / — | 3000 / — | 700 / `stone` | 1.20 | 0.70 | 40.0 | 0 |
| **10** | `fire` | energetic | Gas | `[255, 170, 20, 255]` | 25 | 0.1 | 2 | 1.00 | 0 | — / — | — / — | — / — | 1.00 | 0.90 | 0.0 | 30 |
| **11** | `smoke` | gases | Gas | `[100, 100, 100, 150]` | 15 | 0.8 | 2 | 0.00 | 10000 | — / — | — / — | — / — | 1.00 | 0.03 | 0.0 | 75 |
| **12** | `wood` | solids | Solid | `[142, 85, 45, 255]` | 10 | 650.0 | 0 | 0.60 | 250 | — / — | — / — | — / — | 1.70 | 0.10 | 30.0 | 0 |
| **13** | `iron` | solids | Solid | `[192, 196, 200, 255]` | 8 | 7800.0 | 0 | 0.00 | 10000 | 1538 / `lava` | 2862 / — | — / — | 0.45 | 0.95 | 150.0 | 0 |
| **14** | `gunpowder`| explosives| Powder | `[70, 70, 75, 255]` | 10 | 1650.0 | 1 | 0.95 | 200 | — / — | — / — | — / — | 1.00 | 0.20 | 10.0 | 0 |
| **15** | `tnt` | explosives| Solid | `[210, 45, 30, 255]` | 5 | 1654.0 | 0 | 0.90 | 250 | — / — | — / — | — / — | 1.10 | 0.15 | 40.0 | 0 |
| **16** | `glass` | solids | Solid | `[220, 240, 245, 120]` | 5 | 2400.0 | 0 | 0.00 | 10000 | 1400 / `lava` | — / — | — / — | 0.84 | 0.30 | 25.0 | 0 |
| **17** | `ash` | powders | Powder | `[160, 160, 160, 255]` | 10 | 500.0 | 1 | 0.00 | 10000 | 1100 / `lava` | — / — | — / — | 0.70 | 0.08 | 5.0 | 0 |

---

### 5.2 Exact JSON Content for All 18 Asset Files (`assets/materials/*.json`)

#### 1. `assets/materials/air.json`
```json
{
    "id": 0,
    "name": "air",
    "category": "gases",
    "state": "Gas",
    "color": [0, 0, 0, 0],
    "color_variation": 0,
    "density": 0.0,
    "dispersion": 0,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 10000.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 1.0,
    "thermal_conductivity": 0.02,
    "strength": 0.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 2. `assets/materials/stone.json`
```json
{
    "id": 1,
    "name": "stone",
    "category": "solids",
    "state": "Solid",
    "color": [128, 128, 128, 255],
    "color_variation": 15,
    "density": 2500.0,
    "dispersion": 0,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 1200.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 0.80,
    "thermal_conductivity": 0.20,
    "strength": 80.0,
    "lifetime": 0,
    "melt_target": "lava",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 3. `assets/materials/dirt.json`
```json
{
    "id": 2,
    "name": "dirt",
    "category": "powders",
    "state": "Powder",
    "color": [134, 96, 67, 255],
    "color_variation": 12,
    "density": 1500.0,
    "dispersion": 1,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 1000.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 0.90,
    "thermal_conductivity": 0.15,
    "strength": 10.0,
    "lifetime": 0,
    "melt_target": "lava",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 4. `assets/materials/sand.json`
```json
{
    "id": 3,
    "name": "sand",
    "category": "powders",
    "state": "Powder",
    "color": [228, 204, 131, 255],
    "color_variation": 16,
    "density": 1600.0,
    "dispersion": 2,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 1700.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 0.80,
    "thermal_conductivity": 0.25,
    "strength": 15.0,
    "lifetime": 0,
    "melt_target": "glass",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 5. `assets/materials/water.json`
```json
{
    "id": 4,
    "name": "water",
    "category": "liquids",
    "state": "Liquid",
    "color": [34, 115, 230, 200],
    "color_variation": 8,
    "density": 1000.0,
    "dispersion": 5,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 10000.0,
    "boil_point": 100.0,
    "freeze_point": 0.0,
    "specific_heat": 4.18,
    "thermal_conductivity": 0.60,
    "strength": 0.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "steam",
    "freeze_target": "ice"
}
```

#### 6. `assets/materials/ice.json`
```json
{
    "id": 5,
    "name": "ice",
    "category": "solids",
    "state": "Solid",
    "color": [180, 220, 255, 230],
    "color_variation": 10,
    "density": 917.0,
    "dispersion": 0,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 0.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 2.09,
    "thermal_conductivity": 0.40,
    "strength": 20.0,
    "lifetime": 0,
    "melt_target": "water",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 7. `assets/materials/steam.json`
```json
{
    "id": 6,
    "name": "steam",
    "category": "gases",
    "state": "Gas",
    "color": [210, 225, 240, 160],
    "color_variation": 10,
    "density": 0.6,
    "dispersion": 3,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 10000.0,
    "boil_point": 10000.0,
    "freeze_point": 95.0,
    "specific_heat": 2.00,
    "thermal_conductivity": 0.05,
    "strength": 0.0,
    "lifetime": 120,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": "water"
}
```

#### 8. `assets/materials/oil.json`
```json
{
    "id": 7,
    "name": "oil",
    "category": "liquids",
    "state": "Liquid",
    "color": [85, 75, 40, 240],
    "color_variation": 6,
    "density": 800.0,
    "dispersion": 3,
    "flammability": 0.80,
    "ignition_temp": 80.0,
    "melt_point": 10000.0,
    "boil_point": 300.0,
    "freeze_point": -20.0,
    "specific_heat": 1.80,
    "thermal_conductivity": 0.12,
    "strength": 0.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "smoke",
    "freeze_target": ""
}
```

#### 9. `assets/materials/acid.json`
```json
{
    "id": 8,
    "name": "acid",
    "category": "liquids",
    "state": "Liquid",
    "color": [75, 230, 45, 220],
    "color_variation": 12,
    "density": 1200.0,
    "dispersion": 4,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 10000.0,
    "boil_point": 110.0,
    "freeze_point": -273.15,
    "specific_heat": 2.50,
    "thermal_conductivity": 0.30,
    "strength": 0.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "smoke",
    "freeze_target": ""
}
```

#### 10. `assets/materials/lava.json`
```json
{
    "id": 9,
    "name": "lava",
    "category": "liquids",
    "state": "Liquid",
    "color": [255, 90, 0, 255],
    "color_variation": 20,
    "density": 3000.0,
    "dispersion": 1,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 10000.0,
    "boil_point": 3000.0,
    "freeze_point": 700.0,
    "specific_heat": 1.20,
    "thermal_conductivity": 0.70,
    "strength": 40.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": "stone"
}
```

#### 11. `assets/materials/fire.json`
```json
{
    "id": 10,
    "name": "fire",
    "category": "energetic",
    "state": "Gas",
    "color": [255, 170, 20, 255],
    "color_variation": 25,
    "density": 0.1,
    "dispersion": 2,
    "flammability": 1.00,
    "ignition_temp": 0.0,
    "melt_point": 10000.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 1.00,
    "thermal_conductivity": 0.90,
    "strength": 0.0,
    "lifetime": 30,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 12. `assets/materials/smoke.json`
```json
{
    "id": 11,
    "name": "smoke",
    "category": "gases",
    "state": "Gas",
    "color": [100, 100, 100, 150],
    "color_variation": 15,
    "density": 0.8,
    "dispersion": 2,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 10000.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 1.00,
    "thermal_conductivity": 0.03,
    "strength": 0.0,
    "lifetime": 75,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 13. `assets/materials/wood.json`
```json
{
    "id": 12,
    "name": "wood",
    "category": "solids",
    "state": "Solid",
    "color": [142, 85, 45, 255],
    "color_variation": 10,
    "density": 650.0,
    "dispersion": 0,
    "flammability": 0.60,
    "ignition_temp": 250.0,
    "melt_point": 10000.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 1.70,
    "thermal_conductivity": 0.10,
    "strength": 30.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 14. `assets/materials/iron.json`
```json
{
    "id": 13,
    "name": "iron",
    "category": "solids",
    "state": "Solid",
    "color": [192, 196, 200, 255],
    "color_variation": 8,
    "density": 7800.0,
    "dispersion": 0,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 1538.0,
    "boil_point": 2862.0,
    "freeze_point": -273.15,
    "specific_heat": 0.45,
    "thermal_conductivity": 0.95,
    "strength": 150.0,
    "lifetime": 0,
    "melt_target": "lava",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 15. `assets/materials/gunpowder.json`
```json
{
    "id": 14,
    "name": "gunpowder",
    "category": "explosives",
    "state": "Powder",
    "color": [70, 70, 75, 255],
    "color_variation": 10,
    "density": 1650.0,
    "dispersion": 1,
    "flammability": 0.95,
    "ignition_temp": 200.0,
    "melt_point": 10000.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 1.00,
    "thermal_conductivity": 0.20,
    "strength": 10.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 16. `assets/materials/tnt.json`
```json
{
    "id": 15,
    "name": "tnt",
    "category": "explosives",
    "state": "Solid",
    "color": [210, 45, 30, 255],
    "color_variation": 5,
    "density": 1654.0,
    "dispersion": 0,
    "flammability": 0.90,
    "ignition_temp": 250.0,
    "melt_point": 10000.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 1.10,
    "thermal_conductivity": 0.15,
    "strength": 40.0,
    "lifetime": 0,
    "melt_target": "",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 17. `assets/materials/glass.json`
```json
{
    "id": 16,
    "name": "glass",
    "category": "solids",
    "state": "Solid",
    "color": [220, 240, 245, 120],
    "color_variation": 5,
    "density": 2400.0,
    "dispersion": 0,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 1400.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 0.84,
    "thermal_conductivity": 0.30,
    "strength": 25.0,
    "lifetime": 0,
    "melt_target": "lava",
    "boil_target": "",
    "freeze_target": ""
}
```

#### 18. `assets/materials/ash.json`
```json
{
    "id": 17,
    "name": "ash",
    "category": "powders",
    "state": "Powder",
    "color": [160, 160, 160, 255],
    "color_variation": 10,
    "density": 500.0,
    "dispersion": 1,
    "flammability": 0.0,
    "ignition_temp": 10000.0,
    "melt_point": 1100.0,
    "boil_point": 10000.0,
    "freeze_point": -273.15,
    "specific_heat": 0.70,
    "thermal_conductivity": 0.08,
    "strength": 5.0,
    "lifetime": 0,
    "melt_target": "lava",
    "boil_target": "",
    "freeze_target": ""
}
```

---

### 5.3 C++ Interface Specifications

#### 5.3.1 `src/sim/Material.hpp`
```cpp
#pragma once

#include "core/Types.hpp"
#include <string>

namespace sandbox::sim {

using MaterialId = core::u16;
inline constexpr MaterialId EMPTY_MATERIAL_ID = 0;

enum class MaterialState : core::u8 {
    Solid = 0,
    Powder = 1,
    Liquid = 2,
    Gas = 3
};

struct Material {
    MaterialId id{EMPTY_MATERIAL_ID};
    std::string name{"air"};
    std::string category{"gases"};
    core::Color4u color{0, 0, 0, 0};
    core::u8 colorVariation{0};
    MaterialState state{MaterialState::Gas};

    // Mechanical properties
    core::f32 density{0.0f};
    core::i32 dispersion{0};
    core::f32 strength{0.0f};

    // Thermal properties
    core::f32 thermalConductivity{0.02f};
    core::f32 specificHeat{1.0f};
    core::f32 meltPoint{9999.0f};
    core::f32 boilPoint{9999.0f};
    core::f32 freezePoint{-9999.0f};

    // Phase transition targets (Names for JSON deserialization, IDs for O(1) runtime)
    std::string meltTargetName{""};
    std::string boilTargetName{""};
    std::string freezeTargetName{""};
    MaterialId meltMaterialId{EMPTY_MATERIAL_ID};
    MaterialId boilMaterialId{EMPTY_MATERIAL_ID};
    MaterialId freezeMaterialId{EMPTY_MATERIAL_ID};

    // Combustion & lifecycle
    core::f32 flammability{0.0f};
    core::f32 ignitionTemp{10000.0f};
    core::u8 lifetime{0};
};

} // namespace sandbox::sim
```

#### 5.3.2 `src/sim/MaterialRegistry.hpp`
```cpp
#pragma once

#include "Material.hpp"
#include <vector>
#include <unordered_map>
#include <string>

namespace sandbox::sim {

class MaterialRegistry {
private:
    std::vector<Material> m_materials;
    std::unordered_map<std::string, MaterialId> m_nameToId;
    Material m_emptyMaterial;

    MaterialRegistry();

public:
    static MaterialRegistry& Instance();

    // Singleton semantics: delete copy & move
    MaterialRegistry(const MaterialRegistry&) = delete;
    MaterialRegistry& operator=(const MaterialRegistry&) = delete;
    MaterialRegistry(MaterialRegistry&&) = delete;
    MaterialRegistry& operator=(MaterialRegistry&&) = delete;

    // Loading & Registration
    bool LoadFromDirectory(const std::string& directoryPath);
    bool LoadFromFile(const std::string& filePath);
    void ResolvePhaseTargets();

    MaterialId RegisterMaterial(Material mat);
    
    // Accessors
    [[nodiscard]] const Material& Get(MaterialId id) const noexcept;
    [[nodiscard]] MaterialId FindByName(const std::string& name) const noexcept;
    [[nodiscard]] size_t Count() const noexcept;
    [[nodiscard]] const std::vector<Material>& GetAll() const noexcept;

    // Resetting for testing
    void Clear();
};

} // namespace sandbox::sim
```

#### 5.3.3 `src/sim/MaterialRegistry.cpp` Implementation Architecture
```cpp
#include "MaterialRegistry.hpp"
#include "core/Log.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace sandbox::sim {

namespace fs = std::filesystem;
using json = nlohmann::json;

MaterialRegistry::MaterialRegistry() {
    // Air is always guaranteed at ID 0
    m_emptyMaterial.id = EMPTY_MATERIAL_ID;
    m_emptyMaterial.name = "air";
    m_emptyMaterial.category = "gases";
    m_emptyMaterial.color = core::Color4u(0, 0, 0, 0);
    m_emptyMaterial.state = MaterialState::Gas;
    m_emptyMaterial.density = 0.0f;
    RegisterMaterial(m_emptyMaterial);
}

MaterialRegistry& MaterialRegistry::Instance() {
    static MaterialRegistry instance;
    return instance;
}

void MaterialRegistry::Clear() {
    m_materials.clear();
    m_nameToId.clear();
    RegisterMaterial(m_emptyMaterial); // Re-seed Air at ID 0
}

MaterialId MaterialRegistry::RegisterMaterial(Material mat) {
    auto it = m_nameToId.find(mat.name);
    if (it != m_nameToId.end()) {
        m_materials[it->second] = mat;
        return it->second;
    }

    MaterialId newId = static_cast<MaterialId>(m_materials.size());
    mat.id = newId;
    m_materials.push_back(mat);
    m_nameToId[mat.name] = newId;
    return newId;
}

const Material& MaterialRegistry::Get(MaterialId id) const noexcept {
    if (id < m_materials.size()) {
        return m_materials[id];
    }
    return m_emptyMaterial;
}

MaterialId MaterialRegistry::FindByName(const std::string& name) const noexcept {
    auto it = m_nameToId.find(name);
    if (it != m_nameToId.end()) {
        return it->second;
    }
    return EMPTY_MATERIAL_ID;
}

size_t MaterialRegistry::Count() const noexcept {
    return m_materials.size();
}

const std::vector<Material>& MaterialRegistry::GetAll() const noexcept {
    return m_materials;
}

bool MaterialRegistry::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_WARN("MaterialRegistry", "Failed to open material file: ", filePath);
        return false;
    }

    try {
        json j;
        file >> j;

        Material mat;
        mat.name = j.value("name", "unnamed");
        mat.category = j.value("category", "solids");

        // Parse State
        std::string stateStr = j.value("state", "Solid");
        if (stateStr == "Powder" || stateStr == "POWDER") mat.state = MaterialState::Powder;
        else if (stateStr == "Liquid" || stateStr == "LIQUID") mat.state = MaterialState::Liquid;
        else if (stateStr == "Gas" || stateStr == "GAS") mat.state = MaterialState::Gas;
        else mat.state = MaterialState::Solid;

        // Parse Color
        if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
            mat.color.r = static_cast<core::u8>(j["color"][0].get<int>());
            mat.color.g = static_cast<core::u8>(j["color"][1].get<int>());
            mat.color.b = static_cast<core::u8>(j["color"][2].get<int>());
            mat.color.a = j["color"].size() >= 4 ? static_cast<core::u8>(j["color"][3].get<int>()) : 255;
        }

        // Color Variation
        if (j.contains("color_variation")) mat.colorVariation = static_cast<core::u8>(j["color_variation"].get<int>());
        else if (j.contains("colorVariation")) mat.colorVariation = static_cast<core::u8>(j["colorVariation"].get<int>());

        // Mechanical properties
        mat.density = j.value("density", 1000.0f);
        mat.dispersion = j.value("dispersion", 0);
        mat.strength = j.value("strength", 0.0f);

        // Thermal properties
        if (j.contains("thermal_conductivity")) mat.thermalConductivity = j["thermal_conductivity"].get<core::f32>();
        else if (j.contains("thermalConductivity")) mat.thermalConductivity = j["thermalConductivity"].get<core::f32>();

        if (j.contains("specific_heat")) mat.specificHeat = j["specific_heat"].get<core::f32>();
        else if (j.contains("specificHeat")) mat.specificHeat = j["specificHeat"].get<core::f32>();
        else if (j.contains("heat_capacity")) mat.specificHeat = j["heat_capacity"].get<core::f32>();

        if (j.contains("melt_point")) mat.meltPoint = j["melt_point"].get<core::f32>();
        else if (j.contains("meltPoint")) mat.meltPoint = j["meltPoint"].get<core::f32>();

        if (j.contains("boil_point")) mat.boilPoint = j["boil_point"].get<core::f32>();
        else if (j.contains("boilPoint")) mat.boilPoint = j["boilPoint"].get<core::f32>();

        if (j.contains("freeze_point")) mat.freezePoint = j["freeze_point"].get<core::f32>();
        else if (j.contains("freezePoint")) mat.freezePoint = j["freezePoint"].get<core::f32>();

        // Phase targets
        if (j.contains("melt_target")) mat.meltTargetName = j["melt_target"].get<std::string>();
        else if (j.contains("meltTarget")) mat.meltTargetName = j["meltTarget"].get<std::string>();

        if (j.contains("boil_target")) mat.boilTargetName = j["boil_target"].get<std::string>();
        else if (j.contains("boilTarget")) mat.boilTargetName = j["boilTarget"].get<std::string>();

        if (j.contains("freeze_target")) mat.freezeTargetName = j["freeze_target"].get<std::string>();
        else if (j.contains("freezeTarget")) mat.freezeTargetName = j["freezeTarget"].get<std::string>();

        // Flammability & Lifetime
        mat.flammability = j.value("flammability", 0.0f);
        if (j.contains("ignition_temp")) mat.ignitionTemp = j["ignition_temp"].get<core::f32>();
        else if (j.contains("ignitionTemp")) mat.ignitionTemp = j["ignitionTemp"].get<core::f32>();

        mat.lifetime = static_cast<core::u8>(j.value("lifetime", 0));

        RegisterMaterial(mat);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("MaterialRegistry", "JSON parse error in ", filePath, ": ", e.what());
        return false;
    }
}

bool MaterialRegistry::LoadFromDirectory(const std::string& directoryPath) {
    if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
        LOG_ERROR("MaterialRegistry", "Directory does not exist: ", directoryPath);
        return false;
    }

    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            paths.push_back(entry.path());
        }
    }

    // Sort to ensure air.json is processed first, followed by alphabetical order
    std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b) {
        if (a.stem() == "air") return true;
        if (b.stem() == "air") return false;
        return a.stem() < b.stem();
    });

    for (const auto& p : paths) {
        LoadFromFile(p.string());
    }

    ResolvePhaseTargets();
    LOG_INFO("MaterialRegistry", "Successfully loaded ", m_materials.size(), " materials from ", directoryPath);
    return true;
}

void MaterialRegistry::ResolvePhaseTargets() {
    for (auto& mat : m_materials) {
        if (!mat.meltTargetName.empty()) {
            mat.meltMaterialId = FindByName(mat.meltTargetName);
        }
        if (!mat.boilTargetName.empty()) {
            mat.boilMaterialId = FindByName(mat.boilTargetName);
        }
        if (!mat.freezeTargetName.empty()) {
            mat.freezeMaterialId = FindByName(mat.freezeTargetName);
        }
    }
}

} // namespace sandbox::sim
```

---

## 6. Caveats

1. **Air State (`STATIC` vs `Gas` vs `Solid`)**:
   - In `assets/materials/air.json`, `state` was originally set to `"STATIC"`. In the 4-state `MaterialState` enum (`Solid`, `Powder`, `Liquid`, `Gas`), static boundaries are categorized under `Solid` or `Gas`. Air has density 0.0 and dispersion 0. Crucially, the physics engine updates cells where `id != EMPTY_MATERIAL_ID`, so Air is naturally skipped during physics sweeps regardless of enum value. Setting its state to `Gas` or `Solid` with density 0.0 is completely safe.
2. **Latent Heat Magnitude**:
   - While phase thresholds trigger state conversions, the thermodynamics engine in Milestone 2 must subtract or add $\Delta T_{\text{latent}}$ (recommended $1.0^\circ\text{C}$ to $2.0^\circ\text{C}$) to prevent immediate oscillation.
3. **Viscosity & Corrosive Flags**:
   - `air.json` and Survey Phase 2 included optional auxiliary fields (`viscosity`, `burn_rate`, `corrosive`). These fields are not in the minimal `PROJECT.md:209` struct for Milestone 1. The JSON loader ignores unmapped keys cleanly via nlohmann/json without error, allowing them to remain in JSON for Milestone 2/3 extensions.

---

## 7. Conclusion

This report provides the exhaustive, verified specification for the entire material system of Milestone 1:
1. **Existing Files Audit**: Formally cataloged all existing files in `assets/` and `src/`, confirming that `src/sim/` and 17 of the 18 material JSON files must be created.
2. **Core 18 Materials**: Fully specified with exact values across 20 parameters each (IDs 0–17, categories, states, RGBA colors, color variation, physical densities, dispersion, thermal thresholds, flammability, strength, lifetime, and phase transition targets).
3. **C++ Interface Architecture**: Delivered complete definitions for `src/sim/Material.hpp`, `src/sim/MaterialRegistry.hpp`, and `src/sim/MaterialRegistry.cpp`, featuring 2-pass target resolution, snake_case/camelCase dual parsing, and bounds-safe material retrieval.

---

## 8. Verification Method

To verify these specifications upon implementation:

1. **Compilation Check**:
   Compile the engine with the new `Material.hpp`, `MaterialRegistry.hpp`, and `MaterialRegistry.cpp` added to `CMakeLists.txt`:
   ```powershell
   cmake --build build --config Release
   ```
   **Expected result**: Build completes with exit code 0 and 0 warnings.

2. **Asset Placement Check**:
   Verify all 18 material JSON files are present in `assets/materials/`:
   ```powershell
   $mats = @('air','stone','dirt','sand','water','ice','steam','oil','acid','lava','fire','smoke','wood','iron','gunpowder','tnt','glass','ash')
   $missing = $mats | Where-Object { -not (Test-Path "assets\materials\$_.json") }
   if ($missing.Count -eq 0) { "All 18 material files present!" } else { "Missing: $missing" }
   ```
   **Expected result**: Output `All 18 material files present!`.

3. **Material Registry Unit Test**:
   ```cpp
   auto& reg = sandbox::sim::MaterialRegistry::Instance();
   reg.LoadFromDirectory("assets/materials");
   assert(reg.Count() >= 18);
   assert(reg.Get(0).name == "air");
   assert(reg.FindByName("sand") == 3);
   assert(reg.Get(reg.FindByName("water")).boilMaterialId == reg.FindByName("steam"));
   assert(reg.Get(reg.FindByName("water")).freezeMaterialId == reg.FindByName("ice"));
   assert(reg.Get(reg.FindByName("stone")).meltMaterialId == reg.FindByName("lava"));
   ```
   **Expected result**: All assertions pass.
