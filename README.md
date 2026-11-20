# CraftEngine

A native **C++20 + Direct3D 10** voxel engine, built from scratch for **DX10-class hardware**
(Intel Ironlake / GMA HD, GeForce 8/9, Radeon HD 2000/3000) — the machines the Java client
left behind.

> CraftEngine is an original engine. It is **not** a copy of Minecraft code, and it ships **no**
> Mojang assets. Textures, models and sounds can be loaded from a game installation you own;
> the repository itself stays clean.

## Why this exists

Minecraft 26.3 requires a Java 25 runtime with `-Xmx4G`. On a 2010 laptop (i3-M380, 3.8 GB RAM,
Intel HD Ironlake) the JVM cannot even reserve its heap. A renderer backend cannot fix that:
the cost is in the Java object graph and GC, not the draw calls.

CraftEngine takes the other path: a small native engine with explicit memory, explicit threading
and a renderer written directly against the DX10 API the GPU actually supports.

## Current state (v0.1.0)

- Direct3D 10 device + swap chain, SM 4.0 shaders compiled at runtime (`D3DCompile`)
- 16 × 128 × 16 chunk columns, 16 × 16 chunk world (256 × 256 blocks)
- **Greedy meshing** per axis with per-vertex ambient occlusion baked into the mask key
- **Frustum culling** per chunk, back-face-free meshing (hidden faces are never emitted)
- Texture **array** (no atlas bleeding), nearest-neighbour sampling like the original
- Deterministic terrain: value-noise heightmap, beaches, sea level, trees
- Multithreaded meshing (`hardware_concurrency / 2`, i.e. 2 workers on a 2-core CPU)
- Distance fog matched to the sky colour, FPS/chunk/triangle stats in the title bar

## Controls

| Key | Action |
| --- | --- |
| Click | capture mouse |
| `W A S D` | move |
| `Space` / `Ctrl` | up / down |
| `Shift` | sprint |
| `Esc` | release mouse (again to quit) |
| `F1` | wireframe |
| `V` | toggle vsync |

## Build

### Windows (MSVC)

```powershell
cmake -B build -A x64
cmake --build build --config Release
build\Release\CraftEngine.exe
```

### Windows (MinGW-w64)

```powershell
g++ -std=c++20 -O2 -mwindows -municode -DUNICODE -D_UNICODE `
    src\main.cpp src\chunk.cpp src\world.cpp src\renderer.cpp `
    -o CraftEngine.exe -ld3d10 -ldxgi -ld3dcompiler -static -s
```

CI builds every push on `windows-latest` and attaches the `.exe` to each release.

## Performance targets (i3-M380 / Intel HD / 3.8 GB)

| Metric | Target |
| --- | --- |
| Resolution | 1280 × 720 |
| Frame rate | 60 FPS locked (vsync), 90+ unlocked |
| Draw calls | ≤ 256 (one per chunk column) |
| RAM | < 300 MB |
| Startup | < 3 s |

## Roadmap

- **v0.2** — load real textures/models/blockstates from a local 26.3 installation (zip + deflate
  reader, no external deps), block break/place, collision + walking physics with original constants
- **v0.3** — skylight + block light propagation, day/night cycle, smooth lighting
- **v0.4** — chunk streaming around the player, occlusion culling (software depth raster)
- **v0.5** — inventory, hotbar, crafting, UI drawn from the game's own GUI sprites
- **v1.0** — survival loop: mobs, drops, hunger, saving

## License

MIT for the engine. Third-party assets remain under their own licenses and are never redistributed
by this repository.
