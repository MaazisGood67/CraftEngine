# CraftEngine — Master Plan (Java Minecraft → C++/DX10, same-to-same)

Goal: a 1:1 port of the *feel, mechanics, values, visuals* of the Minecraft `26.3.jar`
(client) into native C++20 + Direct3D 10, targeting the user's i3-M380 / Ironlake (DX10.1).
Method: the jar is the **book of truth** — read every value (block data, mobs, recipes,
physics, sounds, textures, fonts) and re-implement it in C++ smartly. No Mojang code is
committed; assets are loaded at runtime from the user's own jar.

---

## Phase 0 — Core engine (DONE)
- [x] DX10 renderer (shaders, texture array, fog, frustum culling)
- [x] World: 16×128×16 chunks, terrain gen, greedy meshing + AO, multithreaded
- [x] ZIP reader (inflate, ZIP64), PNG decoder (real 26.3 textures)
- [x] Input: keyboard/mouse look, fly camera

## Phase 1 — Asset & Audio Layer (IN PROGRESS)
- [ ] Finish PNG decode → real block textures from jar (stone.png etc.)
- [ ] Sound engine (XAudio2 / WinMM) + `.ogg` (Vorbis) decode
- [ ] Load & play: UI clicks, `random/pop`, `random/orb`, ambient cave, levelup
- [ ] Font engine: load `font/` TTF + `minecraftfont.png` glyph atlas → text renderer
- [ ] Asset registry: resolve all jar paths once (blocks/items/gui/sounds/shaders)

## Phase 2 — Game States & Menus (the "front book cover")
- [ ] GameState: Loading → TitleScreen → (Singleplayer / Options / Quit)
- [ ] Loading screen: "Building terrain" / progress bar + Mojang-style splash text
- [ ] Title menu: logo, panorama background (slow rotating world render), buttons
- [ ] Buttons with hover/focus/click SFX from jar; keyboard + mouse navigation
- [ ] Options: FOV, render distance, vsync, mouse sens, volume (saved to options.txt)
- [ ] Pause menu (Esc): Back / Options / Save & Quit

## Phase 3 — Block/Item Registry (the "dictionary")
- [ ] Port block IDs/states from jar `block/` + `blockstates/` (names, hardness, tools)
- [ ] Item registry (`item/`), textures, names
- [ ] Hotbar (9 slots), creative inventory screen, survival inventory screen
- [ ] Item drop / pickup / stacking (64), tool durability

## Phase 4 — Player & Physics (same values as MC)
- [ ] AABB player (0.6×1.8), step-up, auto-jump off
- [ ] Gravity 32 b/s², jump velocity, walk 4.317 b/s, sprint, sneak, swim, fly
- [ ] Collision vs world (voxel sweep), fall damage, water physics, lava
- [ ] Survival stats: health 20 (10 hearts), hunger 20 (10 shanks), XP bar
- [ ] Third-person camera (F5), hand rendering

## Phase 5 — Interaction
- [ ] Raycast targeting (block outline), break progress per block hardness
- [ ] Break/place blocks + particles + break/dig/place sounds from jar
- [ ] Right-click: use item, eat, place block, open crafting
- [ ] 2×2 inventory crafting + 3×3 crafting table, smelting (furnace)

## Phase 6 — World Systems
- [ ] Chunk streaming: load/unload around player, save/load (NBT-ish format)
- [ ] Day/night cycle, sky color, sun/moon, stars, weather (rain)
- [ ] Light propagation (block + sky light), smooth lighting, AO
- [ ] Structures: villages, dungeons, ravines, caves (port generation rules)

## Phase 7 — Mobs (same models + AI + stats from jar)
- [ ] Passive: pig, cow, sheep, chicken, horse, wolf, cat, villager
- [ ] Hostile: zombie, skeleton, creeper, spider, witch, slime, enderman
- [ ] Mob model format (port `models/`), textures, animations, sounds
- [ ] AI: wander, pathfind, aggro, attack, flee, breeding, taming

## Phase 8 — Combat
- [ ] Melee: attack cooldown, knockback, criticals, sweep
- [ ] Health/damage/armor values per mob (from jar)
- [ ] Ranged: bow (charge, arrow physics), potions, TNT, fishing rod
- [ ] Death, respawn, hardcore

## Phase 9 — The End (game "ending")
- [ ] Nether portal + Nether dimension (biomes, nether fortress)
- [ ] Stronghold + End portal (eye of ender)
- [ ] The End: obsidian pillars, crystals, endermen
- [ ] Ender Dragon boss: AI phases, health bar, dragon fireball, end crystals
- [ ] End credits scroll + "The End?" → respawn to overworld

## Phase 10 — Polish (match the original feel)
- [ ] F3 debug screen, F1 hide HUD, F11 fullscreen
- [ ] Multiplayer-ready architecture (save sync, later: LAN)
- [ ] Performance pass on target HW (60–90 FPS idle @1280×720)

---
Every phase ends with: build → run → verify against jar values → commit → release.