# CraftEngine - agent map

Minecraft: Java Edition 26.3 rewritten in C++20 on Direct3D 10. The vanilla jar is the
specification. Read `.config/opencode/skills/vanilla-parity/SKILL.md` before changing
gameplay.

## Rules that bite

- **Never commit Mojang files.** Jars, assets, decompiled source and dumps live in
  `.local/` or `vanilla/`, both gitignored, rebuilt from the user's own installation.
- **No invented values.** Every gameplay constant is in `src/vanilla.h` or comes from a
  jar data file. If it is missing, extract it first.
- **Vanilla order of operations.** Same constants, same branches, same RNG draws.
- **DX10 only.** `ID3D10Device`, SM4.0 shaders. The target machine is Intel Ironlake
  (DX10.1, 3.8 GB RAM); stay inside that budget.
- **Ticks are 20 Hz.** Physics and AI tick at `Vanilla::TICK`; rendering interpolates.
- **Evidence, not claims.** Build green + harness pass + screenshot or log line.

## Layout

| Path | Owns |
|---|---|
| `src/main.cpp` | Win32 loop, state machine (Loading/Title/Options/InGame/Pause), HUD, input, world interaction |
| `src/vanilla.h` | All vanilla gameplay constants (physics, reach, combat, block hardness, mob stats, dragon) |
| `src/vanilla_data.*` | Runtime loader for jar data: registries, keybinds, worldgen settings |
| `src/rng.*` | Java 26.3 Xoroshiro/Legacy random streams, bit-exact (rng harness) |
| `src/blocks.h` | Block ids, names, tiles, hardness, tool requirements |
| `src/chunk.*` | 16x128x16 chunk storage + greedy mesher with per-vertex AO |
| `src/world.*` | Terrain generation, chunk grid, block get/set, height query |
| `src/renderer.*` | DX10 device, chunk buffers, texture array, terrain/entity pipelines, frustum |
| `src/textures.*` | Jar texture -> 16x16 tile mapping, canonical paths, biome tints |
| `src/gui.*` | 2D sprite batcher, 9-slice widgets, panorama cube |
| `src/font.*` | `ascii.png` bitmap font with shadow and rotation |
| `src/assets.*` | Jar + asset-index object store access |
| `src/zip.*`, `src/png.*`, `src/json.h` | ZIP64, PNG (palette/tRNS/bit depth), JSON |
| `src/sound.*` | waveOut mixer + stb_vorbis |
| `src/player.*` | Player physics, collision, raycast |
| `src/inventory.*` | Item stacks, hotbar/main/armor/offhand, crafting |
| `src/combat.*` | Damage, armour, knockback, crits, cooldown, fall damage |
| `src/mobs.*` | Mob types, vanilla stats, goal AI, spawning |
| `src/achievements.*` | Advancements/toasts |
| `tools/` | Build, oracle, parity harnesses, screenshots, port ledger |
| `.local/` | Generated data and captures (gitignored) |

## Build and test

```powershell
powershell -File tools/build-local.ps1      # MinGW-w64 local build
powershell -File tools/dump-vanilla.ps1     # registries/keybinds/worldgen from the jar
powershell -File tools/port-ledger.ps1 status
powershell -File tools/parity.ps1 status
```

Kill `CraftEngine.exe` before rebuilding (the linker cannot replace a running exe).

## Parity workflow

1. `tools/port-ledger.ps1 where Class.method` to find the port (or prove it is missing).
2. Read the vanilla method with REA; note constants, RNG draws and branch order.
3. Port it, cite the vanilla method in a comment, add the ledger row.
4. Run the harness (`tools/parity.ps1`) and compare against the Java oracle.
5. For UI: capture vanilla and CraftEngine at the same resolution and compare.

Horizon reference (methodology only, LGPL Rust code is not copied):
`horizon-rust/horizon`, `.claude/skills/vanilla-parity/SKILL.md`,
`docs/parity-workflow.md`.
