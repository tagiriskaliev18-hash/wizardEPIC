# wizardEPIC — Арена стихий

Third-person spell-casting sandbox with massive destruction, built on **Unreal Engine 5.8.4**.

Cast four elemental spells — **Fire**, **Earth**, **Wind**, **Lightning** — and demolish an arena full of destructible structures, props, and crystal targets. Everything is procedurally generated and data-driven.

![UE 5.8](https://img.shields.io/badge/Unreal_Engine-5.8.4-313131?logo=unrealengine)
![C++](https://img.shields.io/badge/C++-20-00599C?logo=cplusplus)
![Python](https://img.shields.io/badge/Python-3.10+-3776AB?logo=python)

---

## 🏗️ Project Structure

```
wizardEPIC/
├── Config/                         # UE project configs
├── Content/                        # UE content (imported by Forge)
│   └── Wizard/                     # Generated game content
├── Source/WizardEPIC/              # C++ game module
│   ├── Public/                     # Headers (types, classes)
│   └── Private/                    # Implementation
├── Scripts/forge/                  # Content generation pipeline
│   ├── contract.py                 # Shared asset names & data contract
│   ├── offline_forge.py            # Offline asset generator (Python stdlib)
│   ├── ue_forge.py                 # UE editor import & level assembly
│   └── Generated/                  # Output (textures, audio, meshes, layout)
├── Run_Forge.bat                   # One-click: generate + import assets
├── Play_WizardEPIC.bat             # One-click: launch the game
├── wizardEPIC.uproject             # UE project file
└── README.md                       # This file
```

---

## 🔧 Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| Unreal Engine | **5.8.4** | Installed at `C:\Program Files\Epic Games\UE_5.8` |
| Visual Studio / Build Tools | 2022+ | C++20, Windows SDK |
| Python | **3.10+** | For the content forge (stdlib only — no pip packages) |

Python is detected automatically from `uv` or system PATH.

---

## 🚀 Build & Run (Step by Step)

### 1. Generate C++ project files

```bat
cd C:\Users\user\Desktop\wizardEPIC
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" ^
  -projectfiles -project="%CD%\wizardEPIC.uproject" -game -rocket -progress
```

Or open the `.uproject` in the editor — it generates project files automatically.

### 2. Build C++ module

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ^
  WizardEPICEditor Win64 Development "%CD%\wizardEPIC.uproject" -waitmutex
```

### 3. Generate & import content (Forge)

```bat
Run_Forge.bat
```

This runs two stages:
1. **`offline_forge.py`** — Generates ~200 assets (textures, audio, meshes, fracture pieces) into `Scripts/forge/Generated/` using pure Python (no packages needed).
2. **`ue_forge.py`** — Imports everything into the UE editor, creates materials, spell DataAssets, destructible definitions, and assembles the arena level `L_Arena`.

> **Note:** The script waits for any running C++ build (`UnrealBuildTool.exe`) before launching the editor (8 GB RAM machine constraint).

### 4. Play!

```bat
Play_WizardEPIC.bat
```

Or open the project in the editor and press **Play**.

---

## 🎮 Controls

| Input | Action |
|-------|--------|
| `1` / `2` / `3` / `4` | Select spell: Fire / Earth / Wind / Lightning |
| Mouse Wheel | Cycle spells |
| **LMB** (tap) | Cast spell |
| **LMB** (hold) | Charge spell (bigger blast at release) |
| `Shift` | Sprint |
| `Space` | Dash |
| `R` | Reset arena (restore all structures) |
| `H` | Toggle hint overlays |
| `Escape` | Toggle mouse capture |

---

## ⚡ Spell System

| Spell | Element | Delivery | Description |
|-------|---------|----------|-------------|
| **Fireball** | Fire 🔥 | Projectile | Fast projectile that detonates on contact |
| **Earthen Wedge** | Earth 🪨 | Ground Line | Pillars erupt from the ground in a line |
| **Storm's Echo** | Wind 💨 | Cone | Short-range directional gust |
| **Lightning** | Lightning ⚡ | Strike | Instant bolt strikes at the aim point |

Each spell has:
- **Tap cast** — instant at base power
- **Charged cast** — hold LMB for up to 2.5× power, radius, and FX
- **Cooldown** — per-spell timer
- **Material multipliers** — different elements are more/less effective against different materials

---

## 💥 Destruction System

- **Structures** (walls, pillars, arches, towers) have individually simulated fracture pieces with a support graph
- **Progressive collapse** — breaking a support piece causes pieces above it to fall in a staggered cascade
- **Breakable props** (pots, crates, barrels, rocks, trees) shatter into debris
- **Crystal targets** — 15 glowing crystals worth bonus points, respawn after 22 seconds
- **Scoring** — points per broken piece, combo multiplier for rapid destruction, collapse bonuses

---

## 🔨 Content Forge Pipeline

The forge is a two-stage pipeline that generates all game content procedurally:

### Stage 1: `offline_forge.py` (runs on any machine)

Uses **only Python stdlib** (no numpy, no Pillow):
- 11 FX sprite textures (smoke, dust, sparks, runes...)
- 6 environment textures (ground, rock, sandstone, wood, crystal)
- 4 spell icon textures (256×256 with element glyphs)
- 7 FX meshes (quad, sphere, ring, pillar, bolt, chip, shard)
- 8 environment meshes (terrain bowl, cliff ring, grass, bushes, ruins)
- 15 destructible shapes × 4–20 Voronoi fracture pieces each
- 22 sound effects (spells, breaks, UI) + ambient loop
- `layout.json` — complete arena layout data

### Stage 2: `ue_forge.py` (runs inside UE editor)

- Imports all generated assets into `/Game/Wizard/`
- Creates 30+ materials (environment, FX, UI, decals)
- Creates 4 `USpellDataAsset` instances with full tuning
- Creates 15 `UDestructibleDefinition` DataAssets with piece data
- Assembles the arena level with lighting, terrain, structures, props, crystals, vegetation

### Re-running the forge

```bat
Run_Forge.bat
```

It's safe to re-run — all assets are overwritten/updated.

---

## 📂 Data Contract

The single source of truth for all shared asset names is:
- **C++ side:** `Source/WizardEPIC/Public/WizardTypes.h` → `namespace WizardPaths`
- **Python side:** `Scripts/forge/contract.py`
- **Documentation:** `.sdd/api/CONTENT_CONTRACT.md`

All C++ code loads assets by soft path — missing content logs a warning but doesn't crash.

---

## 📋 .gitignore Notes

The following are **excluded** from version control:
- `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `Build/` — UE build output
- `.sdd/` — orchestration scratch workspace
- `Scripts/forge/Generated/` — regenerated by `offline_forge.py`
- `.vs/`, `*.sln`, `*.pdb` — IDE/build files

---

## 📜 License

Private project. All rights reserved.
