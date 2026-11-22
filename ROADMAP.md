# CraftEngine roadmap - the Horizon method

Minecraft: Java Edition **26.3** is the specification. The user's own jar is the oracle.
Parity means *the same output for the same input*, proven by a differential harness, not
"it looks Minecrafty". This document is the A-to-Z plan, copied in shape from
`horizon-rust/horizon` (`docs/rust-parity.md`, `docs/parity-workflow.md`,
`.claude/skills/vanilla-parity/SKILL.md`).

## What Horizon actually did first

Horizon's own status document lists their build order. It is **bottom-up**, not menus-first:

1. **Ground truth** - pin 26.3, decompile into `.local/`, build Java oracles, build the
   port ledger, add a check that rejects Mojang files in the repo.
2. **Positions and arithmetic** - block/section/chunk packing, Java `int`/`long` semantics,
   negative-coordinate flooring, AABBs, ray traversal.
3. **Random** - Legacy `Random` and `XoroshiroRandom`, every draw form, fork/reseed,
   UTF-16 string hashing.
4. **Noise** - Perlin, simplex, smeared Perlin, `ImprovedNoise`, `NormalNoise`, octave
   weights, modern and legacy Nether seeding.
5. **Density functions** - constants, gradients, shifted noise, all unary/binary operators,
   caches, slices, splines, surface searches, range analysis, graph compilation.
6. **World data** - NBT, region files, palettes, resident sections, heightmaps, scheduled ticks.
7. **Worldgen codecs** - every bundled noise/density/biome JSON decoded by the real codec
   and compared.
8. **Biomes** - climate sampling, multi-noise search, zoom, chunk biome storage.
9. **Terrain** - aquifers, noise fill, surface rules, carving masks and carvers.
10. **Features and structures** - placement modifiers, ores, trees, disks, springs, etc.
11. **Gameplay kernels** - collision, movement, fluid, damage, hunger, items, effects.
12. **Client** - renderer, models, lighting, UI, sound, input.
13. **Integration** - live world, saves, dimensions, multiplayer.
14. **Performance last**, measured on matched scenes after behaviour is proven.

They publish an exact matching-line count per harness (e.g. 15,524,627 lines for density)
and explicitly say a partial port is **not** "100% parity". We copy that honesty.

## Our starting point (honest)

The current build is a **vertical slice**, not a clone. These parts are placeholders and
will be replaced, not polished:

| Area | Today | Target |
|---|---|---|
| Terrain | `World::heightAt` sine/hash noise, 256x256x128, sea level 24 | 26.3 noise settings, density graph, aquifers, surface rules, sea level 63 |
| Biomes | none | multi-noise biome source from `overworld.json` |
| Features/structures | hand-placed trees | placed features + structures from the jar |
| Mobs | coloured boxes, simple chase AI | vanilla models, goals, attributes, loot |
| GUI | title/pause/options + HUD; inventory key does nothing visible | every vanilla screen |
| Items | 9 hotbar block items, 4 recipes | all 1,658 items, 2,042 recipes, components |
| Saves | none | Anvil NBT/region, world list |
| Dimensions | overworld only | Nether, End, portals, dragon |
| Sounds | click/pop/dig/levelup | all 4,961 sound events with vanilla subtitles |

The engine shell (DX10 device, chunk mesher, GUI batcher, font, OGG mixer, ZIP/PNG/JSON)
is reusable and stays.

## A-to-Z phases

Every phase ends with a **harness** and a **visible checkpoint**. No phase is "done"
because it compiles.

### Phase 0 - Ground truth (DONE)
- `tools/vanilla-env.ps1` resolves the install, Java 25, classpath, natives, assets.
- `tools/dump-vanilla.ps1` runs `VanillaDump` against the real jar: 7,053 registry
  entries, block states, item props, keybinds.
- `tools/port_map.tsv` + `tools/port-ledger.ps1` answer "where is vanilla `Class.method`?"
- `AGENTS.md` + `.config/opencode/skills/vanilla-parity/SKILL.md`.
- `.gitignore` rejects `vanilla/`, `.local/`, jars and dumps.
- **Proof:** `vanilla/registry.json` has 1,286 blocks, 1,658 items, 161 entity types.

### Phase 1 - Deterministic core (IN PROGRESS)
1.1 Java `Random` + `XoroshiroRandom`: all draw forms, bounded int, fork, seed expansion.
    **DONE** - `src/rng.*`, oracle `tools/oracle/RngOracle.java`, probe `tools/probe/probe.cpp`;
    `tools/parity.ps1 run rng` passes (324 scripted entries, raw float/double bits).
1.2 `ImprovedNoise`, `PerlinNoise`, `NormalNoise` (octaves, weights, normalization). 26.3 ships
    `Noise`, `GradientNoise`, `NoiseStack`, `SmearedPerlinNoise`, `NormalNoise`; port from bytecode.
1.3 Density functions: read `data/minecraft/worldgen/density_function/*.json`, compile the
    graph, implement every operator and cache.
1.4 `overworld.json` noise settings: `noise_router`, `sea_level`, `aquifers_enabled`,
    `default_fluid`, `noise.size_vertical/horizontal`.
- **Harness:** `tools/oracle/WorldgenOracle.java` vs `CraftEngineProbe.exe --probe worldgen`,
  fixed seeds, 100k+ sampled values, `%.6f`.
- **Visible:** a heightmap + biome map rendered from the same seed as Java.

### Phase 2 - Chunk generation
2.1 Noise fill (density -> block states), 3D biomes.
2.2 Aquifers and fluid picker.
2.3 Surface rules (grass/sand/gravel/snow/terracotta), heightmaps.
2.4 Carvers (caves, canyons).
2.5 Features (ores, trees, grass, flowers, lakes, springs, disks, geodes...).
2.6 Structures (villages, temples, strongholds, mineshafts).
- **Harness:** full 16x384x16 block array hashes for 20 seeds x 8 chunks, plus heightmaps.
- **Visible:** same seed produces the same terrain as Java, side by side.

### Phase 3 - Renderer parity
3.1 Blockstate/model JSON from the jar; cube/cross/tinted cross models.
3.2 Vanilla lighting (sky + block light, smooth light, AO).
3.3 Sky, clouds, sun/moon, stars, fog, underwater fog.
3.4 Held item + hand, player model (Steve/Alex skins), third person.
3.5 Entity models from `geo`/model JSON, animations.
- **Harness:** screenshot pairs at 1280x720 from fixed camera positions; pixel delta report.
- **Visible:** screenshots that can be placed next to Java without embarrassment.

### Phase 4 - Player, input, camera
4.1 Movement exactly: walk/sprint/sneak/fly/swim/ladder, friction, step height, jump.
4.2 Collision and raycast against real block shapes.
4.3 Keybinds from `options.txt` (already wired), mouse sensitivity, raw input.
4.4 Creative double-tap flight, F5 perspective, F3 debug, F1 HUD.
- **Harness:** input-script trace (position/velocity/rotation per tick) vs Java.
- **Visible:** walk the same path in both and end at the same coordinates.

### Phase 5 - GUI parity
5.1 Title screen, splash, panorama, accessibility, language.
5.2 World creation (game mode, seed, cheats, datapacks, world type).
5.3 Options: all vanilla pages and sliders.
5.4 Inventory, crafting table, furnace, chest, creative inventory with tabs/search.
5.5 HUD: hotbar, hearts, hunger, armour, air, XP, effects, crosshair, subtitles.
5.6 Pause, advancements, statistics, social interactions, chat, death screen.
5.7 Toasts, tooltips, item rarity, durability bars.
- **Harness:** screenshot pair per screen + widget hit-test trace.
- **Visible:** every menu is the vanilla texture and layout.

### Phase 6 - Survival systems
6.1 Mining progress, tool tiers, enchantments, correct drops.
6.2 Item entities, pickup, despawn, merging.
6.3 Crafting/smelting/smithing from the jar's 2,042 recipes.
6.4 Containers, hoppers, redstone (later), block entities.
6.5 Hunger, saturation, regeneration, XP, anvil, enchanting.
- **Harness:** recipe resolution + block break/drop traces vs Java.

### Phase 7 - Mobs, combat, spawning
7.1 Entity attributes and movement.
7.2 Goal AI (vanilla goal selectors, pathfinding).
7.3 Combat: cooldown, crits, sweep, armour, enchantments, knockback, shields.
7.4 Spawning: light level, biome, difficulty, mob caps.
7.5 Loot tables from the jar, breeding, villager trades.
- **Harness:** per-tick entity traces (position, motion, rotation, health, target) vs Java.
- **Visible:** a zombie behaves like a zombie.

### Phase 8 - Dimensions and bosses
Nether generation, portals, End islands, ender dragon, wither, raids, structures.

### Phase 9 - Audio, particles, weather, time
All sound events with subtitles, music, particles, rain/snow/thunder, day-night cycle.

### Phase 10 - Saves and world management
Anvil NBT/region read+write, world list, autosave, crash recovery, datapacks.

### Phase 11 - Performance and release
Matched-scene FPS/RAM on the target laptop (i3 M380, Ironlake, 3.8 GB), DX10 budget,
packaging, installer, release notes. Performance work never changes behaviour.

## First ten commits (in order)

1. **Xoroshiro + Legacy RNG + oracle harness** (DONE - `parity.ps1 run rng` PASS).
2. `ImprovedNoise`/`NormalNoise` + noise harness.
3. Density graph compiler + all operators + density harness.
4. `overworld.json` loader (runtime, from the jar).
5. Noise fill -> chunk block array + chunk hash harness.
6. Biomes (climate sampler, multi-noise) + biome harness.
7. Surface rules + heightmaps + surface harness.
8. Carvers + feature placement (ores/trees first) + stage harness.
9. Replace `World::generate` with the vanilla pipeline; keep the mesh path.
10. Screenshot harness: Java vs C++ at identical seed/position.

## The loop for every change

```
1. tools/port-ledger.ps1 where <Class.method>     # find the port, or prove it missing
2. REA / javap the real method                     # constants, branches, RNG order
3. Port it; cite the vanilla method in a comment; add the ledger row
4. tools/oracle/run.ps1 -Case <case>               # Java writes .local/oracle/<case>.json
5. CraftEngineProbe.exe --probe <case>             # C++ writes .local/probe/<case>.json
6. tools/parity.ps1 compare <case>                 # diff, non-zero exit on mismatch
7. tools/build-local.ps1                           # game still builds and runs
8. Screenshot pair for anything visible
```

## Rules that do not bend

- **No Mojang files in the repo.** Jars, assets, decompiled sources, dumps: `.local/`,
  `vanilla/`, both gitignored. `tools/check-public-tree.ps1` runs before commit.
- **No invented gameplay values.** Every constant is in `src/vanilla.h` or read from a jar
  file. If it is missing, extract it first.
- **Same RNG order.** One extra draw shifts the whole world.
- **Java float semantics.** Keep `float` where vanilla uses `float`; do not silently widen.
- **DX10 stays.** The target GPU is DX10.1; no DX11/12 features.
- **Evidence, not claims.** A phase is done when the harness passes and the screenshot exists.
- **Never weaken a harness to pass.** Fix the port or record the real vanilla rule.

## Reference

- `horizon-rust/horizon`: `docs/rust-parity.md`, `docs/port-ledger.md`,
  `docs/parity-workflow.md`, `.claude/skills/vanilla-parity/SKILL.md`.
- Methodology only. Horizon is LGPL-3.0 Rust; no code is copied.

## Progress log

| Date | Commit | Harness | Result |
|---|---|---|---|
| 2026-11-22 | vanilla registry + keybinds from the user's install | `vanilla/registry.json` | 7,053 entries, 62 keybinds |
| 2026-11-22 | Java RNG port | `parity.ps1 run rng` | **PASS** - 324 entries, exact bits |

