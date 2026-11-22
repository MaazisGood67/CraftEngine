// CraftEngine - Minecraft 26.3 client re-implementation (C++20 / Direct3D 10).
//
// Front end flow (same as vanilla):
//   Loading screen (dirt + Mojang logo + progress bar)
//   -> Title screen (rotating panorama, logo, splash, buttons)
//   -> Options (FOV / render distance / sensitivity / sound / vsync)
//   -> Singleplayer -> in-game HUD
//   -> Esc -> Game Menu (pause)
//
// All textures/fonts/sounds are read from the user's own 26.3 installation at
// runtime. No Mojang assets are stored in this repository.
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "achievements.h"
#include "assets.h"
#include "combat.h"
#include "font.h"
#include "gui.h"
#include "inventory.h"
#include "mobs.h"
#include "player.h"
#include "renderer.h"
#include "sound.h"
#include "vanilla.h"
#include "vanilla_data.h"
#include "vmath.h"
#include "world.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int kStartWidth = 1280;
static constexpr int kStartHeight = 720;
static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kFixedTick = 1.0f / 20.0f; // 20 TPS, like vanilla

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
HWND g_hwnd = nullptr;
Renderer g_renderer;
Assets g_assets;
VanillaData g_vanilla;
Gui g_gui;
BitmapFont g_font;
Sound g_sound;
Panorama g_panorama;
World g_world;
Player g_player;
Inventory g_inventory;
CombatState g_combat;
MobManager g_mobs;
AchievementBook g_achievements;
std::vector<ChunkMesh> g_meshes;
std::vector<GpuChunk> g_chunks;

bool g_running = true;
bool g_captured = false;
bool g_keys[256]{};
int g_mouseX = 0;
int g_mouseY = 0;
int g_guiScale = 3;
int g_guiW = 426;
int g_guiH = 240;
double g_time = 0.0;
float g_fps = 0.0f;
float g_fpsAccum = 0.0f;
int g_fpsFrames = 0;

enum class State { Loading, Title, Options, InGame, Pause };
State g_state = State::Loading;
State g_optionsReturn = State::Title;

// Options (options.txt)
float g_fovDeg = 70.0f;
int g_renderDistance = 8;
float g_sensOption = 0.5f; // 0..1
float g_sensitivity = 0.003f;
float g_volume = 1.0f;
bool g_vsync = true;

// Loading
int g_loadStep = 0;
float g_loadProgress = 0.0f;
std::string g_loadText = "Loading Minecraft 26.3";
std::string g_loadError;

// UI
struct UiButton {
    int id = 0;
    std::string label;
    float x = 0, y = 0, w = 0, h = 0;
    bool enabled = true;
    bool slider = false;
    float value = 0.0f; // 0..1 for sliders
};
std::vector<UiButton> g_ui;
int g_activeSlider = -1;

std::map<std::string, int> g_texCache;
std::string g_splash;
float g_panoramaYaw = 0.0f;
bool g_hudHidden = false;
bool g_debug = false;
bool g_debugCam = false;
bool g_inventoryOpen = false;
bool g_creative = true;   // starter-kit world runs in creative until world creation exists
bool g_fullscreen = false;
bool g_thirdPerson = false;
uint64_t g_lastJumpPress = 0;
std::string g_toastTitle;
std::string g_toastDesc;
float g_toastTimer = 0.0f;
int g_lastSlot = -1;
int g_advCount = 0;
int g_lastMouseX = 0;
int g_lastMouseY = 0;
bool g_flying = false;
int g_selectedSlot = 0;
float g_tickAccum = 0.0f;

static const Block kHotbar[9] = {GRASS, DIRT, STONE, COBBLE, PLANKS, LOG, LEAVES, SAND, BEDROCK};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static std::string ExeDir() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring ws(buf);
    const size_t p = ws.find_last_of(L"\\/");
    const std::wstring dir = (p == std::wstring::npos) ? L"." : ws.substr(0, p);
    return std::string(dir.begin(), dir.end());
}

static void logLine(const char* fmt, ...) {
    static bool first = true;
    FILE* f = fopen((ExeDir() + "\\craftengine.log").c_str(), first ? "w" : "a");
    if (!f) return;
    first = false;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void loadOptions() {
    const std::string text = ReadFileBytes(ExeDir() + "\\options.txt");
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        char key[64]{};
        char val[64]{};
        if (sscanf(line.c_str(), "%63[^=]=%63s", key, val) == 2) {
            const std::string k(key);
            if (k == "fov") g_fovDeg = std::clamp(static_cast<float>(atof(val)), 30.0f, 110.0f);
            else if (k == "renderDistance") g_renderDistance = std::clamp(atoi(val), 2, 16);
            else if (k == "sensitivity") g_sensOption = std::clamp(static_cast<float>(atof(val)), 0.0f, 1.0f);
            else if (k == "volume") g_volume = std::clamp(static_cast<float>(atof(val)), 0.0f, 1.0f);
            else if (k == "vsync") g_vsync = atoi(val) != 0;
        }
        start = nl + 1;
    }
    g_sensitivity = 0.001f + g_sensOption * 0.004f;
}

static void saveOptions() {
    FILE* f = fopen((ExeDir() + "\\options.txt").c_str(), "w");
    if (!f) return;
    fprintf(f, "fov=%d\n", static_cast<int>(std::lround(g_fovDeg)));
    fprintf(f, "renderDistance=%d\n", g_renderDistance);
    fprintf(f, "sensitivity=%.3f\n", g_sensOption);
    fprintf(f, "volume=%.3f\n", g_volume);
    fprintf(f, "vsync=%d\n", g_vsync ? 1 : 0);
    fclose(f);
}

static void computeGuiDims(int w, int h) {
    int scale = std::min(w / 320, h / 240);
    g_guiScale = std::max(1, scale);
    g_guiW = (w + g_guiScale - 1) / g_guiScale;
    g_guiH = (h + g_guiScale - 1) / g_guiScale;
}

static float mouseGuiX() { return static_cast<float>(g_mouseX) / g_guiScale; }
static float mouseGuiY() { return static_cast<float>(g_mouseY) / g_guiScale; }

static bool inside(float mx, float my, const UiButton& b) {
    return mx >= b.x && mx < b.x + b.w && my >= b.y && my < b.y + b.h;
}

static int GetTex(const std::string& name) {
    auto it = g_texCache.find(name);
    if (it != g_texCache.end()) return it->second;
    const int tex = LoadPngTexture(g_gui, g_assets, name.c_str());
    if (tex >= 0) g_texCache.emplace(name, tex);
    return tex;
}

static std::string Lang(const char* key, const char* fallback) {
    const std::string& s = g_assets.lang(key);
    return s.empty() ? std::string(fallback) : s;
}

static void playClick() {
    g_sound.play("minecraft/sounds/random/click.ogg", 0.25f * g_volume);
}

// Vanilla KeyMapping lookups (see vanilla_data.cpp; reads the user's options.txt).
static int BindVK(const char* name, int fallback) {
    return g_vanilla.key(name, fallback);
}

static bool BindDown(const char* name, int fallback) {
    const int vk = BindVK(name, fallback);
    return vk > 0 && vk < 256 && g_keys[vk] != 0;
}

static void toggleFullscreen() {
    g_fullscreen = !g_fullscreen;
    if (g_fullscreen) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        RECT rc{0, 0, kStartWidth, kStartHeight};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(g_hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    logLine("fullscreen %d", g_fullscreen ? 1 : 0);
}

static void setCapture(bool on) {
    if (on == g_captured) return;
    g_captured = on;
    if (on) {
        while (ShowCursor(FALSE) >= 0) {}
        RECT rc{};
        GetClientRect(g_hwnd, &rc);
        POINT c{rc.right / 2, rc.bottom / 2};
        g_lastMouseX = c.x;
        g_lastMouseY = c.y;
        ClientToScreen(g_hwnd, &c);
        SetCursorPos(c.x, c.y);
    } else {
        while (ShowCursor(TRUE) < 0) {}
    }
}

// ---------------------------------------------------------------------------
// 2D drawing helpers
// ---------------------------------------------------------------------------
static void drawNine(Gui& gui, int tex, float x, float y, float w, float h, uint32_t color) {
    if (tex < 0) return;
    // widget textures are 200x20 with a 3px nine-slice border.
    const float tw = 200.0f, th = 20.0f, b = 3.0f;
    const float us[4] = {0.0f, b / tw, (tw - b) / tw, 1.0f};
    const float vs[4] = {0.0f, b / th, (th - b) / th, 1.0f};
    const float xs[4] = {x, x + b, x + w - b, x + w};
    const float ys[4] = {y, y + b, y + h - b, y + h};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            gui.quad(tex, xs[col], ys[row], xs[col + 1] - xs[col], ys[row + 1] - ys[row],
                     us[col], vs[row], us[col + 1], vs[row + 1], color);
        }
    }
}

static void drawButton(const UiButton& b, bool hovered) {
    int tex = -1;
    if (!b.enabled) tex = GetTex("minecraft/textures/gui/sprites/widget/button_disabled.png");
    else if (hovered) tex = GetTex("minecraft/textures/gui/sprites/widget/button_highlighted.png");
    else tex = GetTex("minecraft/textures/gui/sprites/widget/button.png");
    drawNine(g_gui, tex, b.x, b.y, b.w, b.h, 0xFFFFFFFF);

    const float tw = g_font.measure(b.label, 1.0f);
    const uint32_t color = b.enabled ? 0xFFFFFFFF : 0xFFA0A0A0;
    g_font.draw(g_gui, b.x + (b.w - tw) * 0.5f, b.y + (b.h - 8.0f) * 0.5f,
                b.label, color, true, 1.0f);
}

static void drawSlider(const UiButton& b, bool hovered) {
    const int bg = GetTex(hovered ? "minecraft/textures/gui/sprites/widget/slider_highlighted.png"
                                  : "minecraft/textures/gui/sprites/widget/slider.png");
    drawNine(g_gui, bg, b.x, b.y, b.w, b.h, 0xFFFFFFFF);
    const int handle = GetTex(hovered ? "minecraft/textures/gui/sprites/widget/slider_handle_highlighted.png"
                                      : "minecraft/textures/gui/sprites/widget/slider_handle.png");
    if (handle >= 0) {
        const float hx = b.x + 2.0f + b.value * (b.w - 12.0f);
        g_gui.quad(handle, hx, b.y, 8.0f, b.h, 0, 0, 1, 1, 0xFFFFFFFF);
    }
    const float tw = g_font.measure(b.label, 1.0f);
    g_font.draw(g_gui, b.x + (b.w - tw) * 0.5f, b.y + (b.h - 8.0f) * 0.5f,
                b.label, 0xFFFFFFFF, true, 1.0f);
}

static void drawUi() {
    const float mx = mouseGuiX();
    const float my = mouseGuiY();
    for (const UiButton& b : g_ui) {
        const bool hovered = b.enabled && inside(mx, my, b);
        if (b.slider) drawSlider(b, hovered);
        else drawButton(b, hovered);
    }
}

static void drawDirtBackground() {
    const int dirt = GetTex("minecraft/textures/block/dirt.png");
    if (dirt >= 0) {
        for (int y = 0; y < g_guiH; y += 32) {
            for (int x = 0; x < g_guiW; x += 32) {
                g_gui.quad(dirt, static_cast<float>(x), static_cast<float>(y), 32, 32,
                           0, 0, 1, 1, 0xFF404040);
            }
        }
    } else {
        g_gui.rect(0, 0, static_cast<float>(g_guiW), static_cast<float>(g_guiH), 0xFF202020);
    }
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------
static void addButton(int id, const std::string& label, float x, float y, float w, float h,
                      bool enabled = true) {
    UiButton b;
    b.id = id;
    b.label = label;
    b.x = x;
    b.y = y;
    b.w = w;
    b.h = h;
    b.enabled = enabled;
    g_ui.push_back(b);
}

static void addSlider(int id, const std::string& label, float x, float y, float w, float h, float v) {
    UiButton b;
    b.id = id;
    b.label = label;
    b.x = x;
    b.y = y;
    b.w = w;
    b.h = h;
    b.slider = true;
    b.value = std::clamp(v, 0.0f, 1.0f);
    g_ui.push_back(b);
}

static void buildTitleUi() {
    g_ui.clear();
    const float cx = g_guiW * 0.5f;
    const float y = g_guiH / 4.0f + 48.0f;
    addButton(100, Lang("menu.singleplayer", "Singleplayer"), cx - 100, y, 200, 20);
    addButton(101, Lang("menu.multiplayer", "Multiplayer"), cx - 100, y + 24, 200, 20, false);
    addButton(102, Lang("menu.online", "Minecraft Realms"), cx - 100, y + 48, 200, 20, false);
    addButton(103, Lang("menu.options", "Options..."), cx - 100, y + 84, 98, 20);
    addButton(104, Lang("menu.quit", "Quit Game"), cx + 2, y + 84, 98, 20);
}

static void buildOptionsUi() {
    g_ui.clear();
    const float cx = g_guiW * 0.5f;
    const float w = 150.0f;
    float y = 52.0f;
    const int fov = static_cast<int>(std::lround(g_fovDeg));
    addSlider(300, "FOV: " + std::to_string(fov), cx - w * 0.5f, y, w, 20,
              (g_fovDeg - 30.0f) / 80.0f);
    y += 24;
    addSlider(301, "Render Distance: " + std::to_string(g_renderDistance), cx - w * 0.5f, y, w, 20,
              (g_renderDistance - 2) / 14.0f);
    y += 24;
    addSlider(302, "Sensitivity: " + std::to_string(static_cast<int>(std::lround(g_sensOption * 100))) + "%",
              cx - w * 0.5f, y, w, 20, g_sensOption);
    y += 24;
    addSlider(303, "Sound: " + std::to_string(static_cast<int>(std::lround(g_volume * 100))) + "%",
              cx - w * 0.5f, y, w, 20, g_volume);
    y += 24;
    addButton(310, std::string("VSync: ") + (g_vsync ? "ON" : "OFF"), cx - w * 0.5f, y, w, 20);
    addButton(320, Lang("gui.done", "Done"), cx - 100, g_guiH - 30.0f, 200, 20);
}

static void buildPauseUi() {
    g_ui.clear();
    const float cx = g_guiW * 0.5f;
    float y = 74.0f;
    addButton(200, Lang("menu.returnToGame", "Back to Game"), cx - 100, y, 200, 20);
    y += 24;
    addButton(201, Lang("menu.options", "Options..."), cx - 100, y, 200, 20);
    y += 24;
    addButton(202, Lang("menu.saveAndQuit", "Save and Quit to Title"), cx - 100, y, 200, 20);
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------
static bool spawnColumnClear(int wx, int wz) {
    const int h = g_world.heightAt(wx, wz);
    if (h <= World::SEA_LEVEL + 1) return false;
    if (g_world.getBlock(wx, h, wz) != GRASS) return false;
    // 5x5 clearing with 5 blocks of head room: no trunk or canopy in the way.
    for (int y = h + 1; y <= h + 5; ++y) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dx = -2; dx <= 2; ++dx) {
                if (g_world.getBlock(wx + dx, y, wz + dz) != AIR) return false;
            }
        }
    }
    // Ground under the whole footprint must be stable (no cliff edge).
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (g_world.heightAt(wx + dx, wz + dz) < h - 1 ||
                g_world.heightAt(wx + dx, wz + dz) > h + 1) return false;
        }
    }
    return true;
}

static void spawnPlayer() {
    int sx = 128;
    int sz = 128;
    bool found = false;
    for (int r = 0; r <= 48 && !found; ++r) {
        for (int dz = -r; dz <= r && !found; ++dz) {
            for (int dx = -r; dx <= r && !found; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                const int wx = 128 + dx;
                const int wz = 128 + dz;
                if (!World::inBounds(wx >> 4, wz >> 4)) continue;
                if (!spawnColumnClear(wx, wz)) continue;
                sx = wx;
                sz = wz;
                found = true;
            }
        }
    }
    const int h = g_world.heightAt(sx, sz);
    g_player.pos = {sx + 0.5f, static_cast<float>(h) + 1.0f, sz + 0.5f};
    g_player.prevPos = g_player.pos;
    g_player.vel = {};
    g_player.yaw = 0.0f;
    g_player.pitch = -0.25f;
    g_player.onGround = false;
    g_player.flying = false;
    g_flying = false;
    g_selectedSlot = 0;
    {
        std::string col;
        for (int y = h - 3; y <= h + 6; ++y) {
            char b[16];
            snprintf(b, sizeof(b), "%d:%d ", y, static_cast<int>(g_world.getBlock(sx, y, sz)));
            col += b;
        }
        logLine("spawn at %d,%d (h=%d, found=%d) column=[%s]", sx, sz, h, found ? 1 : 0, col.c_str());
        const RayHit r = raycast(g_world, g_player.eye(), g_player.forward(), 6.0f);
        logLine("spawn forward ray: hit=%d at %d,%d,%d n=%d,%d,%d", r.hit ? 1 : 0,
                r.x, r.y, r.z, r.nx, r.ny, r.nz);
        const RayHit rd = raycast(g_world, g_player.eye(), Vec3{0, -1, 0}, 6.0f);
        logLine("spawn down ray: hit=%d at %d,%d,%d n=%d,%d,%d", rd.hit ? 1 : 0,
                rd.x, rd.y, rd.z, rd.nx, rd.ny, rd.nz);
    }

    // Starter kit (creative-style so the player can immediately build/fight).
    g_inventory.clear();
    g_inventory.hotbar(0) = {ITEM_WOODEN_PICKAXE, 1, 0};
    g_inventory.hotbar(1) = {ITEM_IRON_PICKAXE, 1, 0};
    g_inventory.hotbar(2) = {ITEM_DIAMOND_SWORD, 1, 0};
    g_inventory.hotbar(3) = {ITEM_TORCH, 64, 0};
    g_inventory.hotbar(4) = {ITEM_BREAD, 16, 0};
    g_inventory.add({ITEM_OAK_PLANKS, 64, 0});
    g_inventory.add({ITEM_COAL, 32, 0});
    g_inventory.armor(1) = {ITEM_ARMOR_CHEST, 1, 0};
    g_inventory.selected = 2;

    g_combat = CombatState{};
    g_achievements.grant(AdvId::OpeningBook);

    // A few passive and hostile mobs near spawn so the systems are visible.
    g_mobs.clear();
    g_mobs.spawn(MobType::Pig, {sx + 6.5f, static_cast<float>(h) + 1.0f, sz + 2.5f});
    g_mobs.spawn(MobType::Pig, {sx + 1.5f, static_cast<float>(h) + 3.5f, sz + 4.5f});
    g_mobs.spawn(MobType::Sheep, {sx + 3.5f, static_cast<float>(h) + 1.0f, sz - 6.5f});
    g_mobs.spawn(MobType::Cow, {sx - 5.5f, static_cast<float>(h) + 1.0f, sz + 8.5f});
    g_mobs.spawn(MobType::Sheep, {sx + 3.5f, static_cast<float>(h) + 1.0f, sz - 6.5f});
    g_mobs.spawn(MobType::Zombie, {sx + 10.5f, static_cast<float>(h) + 1.0f, sz + 12.5f});
    g_mobs.spawn(MobType::Creeper, {sx - 12.5f, static_cast<float>(h) + 1.0f, sz + 7.5f});
}

static void enterGame() {
    g_state = State::InGame;
    g_tickAccum = 0.0f;
    setCapture(true);
}

static void pauseGame() {
    g_state = State::Pause;
    setCapture(false);
    buildPauseUi();
}

static void resumeGame() {
    g_state = State::InGame;
    setCapture(true);
}

static void quitToTitle() {
    g_state = State::Title;
    setCapture(false);
}

static void applySlider(int id, float mx) {
    for (const UiButton& b : g_ui) {
        if (b.id != id || !b.slider) continue;
        float t = (mx - (b.x + 4.0f)) / (b.w - 8.0f);
        t = std::clamp(t, 0.0f, 1.0f);
        switch (id) {
            case 300: g_fovDeg = 30.0f + t * 80.0f; break;
            case 301: g_renderDistance = 2 + static_cast<int>(std::lround(t * 14.0f)); break;
            case 302:
                g_sensOption = t;
                g_sensitivity = 0.001f + t * 0.004f;
                break;
            case 303: g_volume = t; break;
            default: break;
        }
        return;
    }
}

static void handleUiClick(float mx, float my) {
    for (const UiButton& b : g_ui) {
        if (!inside(mx, my, b)) continue;
        if (!b.enabled) return;
        if (b.slider) {
            playClick();
            g_activeSlider = b.id;
            applySlider(b.id, mx);
            return;
        }
        playClick();
        switch (b.id) {
            case 100: enterGame(); break;
            case 103:
                g_optionsReturn = State::Title;
                g_state = State::Options;
                break;
            case 104: g_running = false; break;
            case 200: resumeGame(); break;
            case 201:
                g_optionsReturn = State::Pause;
                g_state = State::Options;
                break;
            case 202: quitToTitle(); break;
            case 310: g_vsync = !g_vsync; break;
            case 320:
                saveOptions();
                g_state = g_optionsReturn;
                if (g_state == State::InGame) setCapture(true);
                break;
            default: break;
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// World rendering
// ---------------------------------------------------------------------------
static void remeshChunk(int cx, int cz) {
    if (!World::inBounds(cx, cz)) return;
    const size_t i = static_cast<size_t>(cz) * World::CX + cx;
    ChunkMesh mesh;
    g_world.chunk(cx, cz).buildMesh(mesh, cx * Chunk::SX, 0, cz * Chunk::SZ);
    const Vec3 mn(static_cast<float>(cx * Chunk::SX), 0.0f, static_cast<float>(cz * Chunk::SZ));
    const Vec3 mx(static_cast<float>((cx + 1) * Chunk::SX), static_cast<float>(Chunk::SY),
                  static_cast<float>((cz + 1) * Chunk::SZ));
    g_renderer.uploadChunk(g_chunks[i], mesh, mn, mx);
}

static void remeshAround(int wx, int wz) {
    const int cx = wx >> 4;
    const int cz = wz >> 4;
    const int lx = wx & 15;
    const int lz = wz & 15;
    remeshChunk(cx, cz);
    if (lx == 0) remeshChunk(cx - 1, cz);
    if (lx == 15) remeshChunk(cx + 1, cz);
    if (lz == 0) remeshChunk(cx, cz - 1);
    if (lz == 15) remeshChunk(cx, cz + 1);
    if (lx == 0 && lz == 0) remeshChunk(cx - 1, cz - 1);
    if (lx == 0 && lz == 15) remeshChunk(cx - 1, cz + 1);
    if (lx == 15 && lz == 0) remeshChunk(cx + 1, cz - 1);
    if (lx == 15 && lz == 15) remeshChunk(cx + 1, cz + 1);
}

static void updateMouseLook() {
    if (!g_captured || g_state != State::InGame) return;
    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    POINT cur{};
    GetCursorPos(&cur);
    ScreenToClient(g_hwnd, &cur);
    const int cx = rc.right / 2;
    const int cy = rc.bottom / 2;
    // Delta from the last observed position, not from the screen centre: warping
    // the cursor each frame would otherwise re-apply the full offset forever.
    const float dx = static_cast<float>(cur.x - g_lastMouseX);
    const float dy = static_cast<float>(cur.y - g_lastMouseY);
    g_lastMouseX = static_cast<int>(cur.x);
    g_lastMouseY = static_cast<int>(cur.y);
    if (dx != 0.0f || dy != 0.0f) {
        g_player.yaw += dx * g_sensitivity;
        g_player.pitch -= dy * g_sensitivity;
        g_player.pitch = std::clamp(g_player.pitch, -1.5533f, 1.5533f);
        POINT c{cx, cy};
        ClientToScreen(g_hwnd, &c);
        SetCursorPos(c.x, c.y);
    }
}

static void tickGame(float dt) {
    g_tickAccum += dt;
    if (g_tickAccum > 0.25f) g_tickAccum = 0.25f;
    while (g_tickAccum >= kFixedTick) {
        PlayerInput in{};
        in.forward = static_cast<float>((BindDown("key.forward", 'W') ? 1 : 0) -
                                        (BindDown("key.back", 'S') ? 1 : 0));
        in.strafe = static_cast<float>((BindDown("key.right", 'D') ? 1 : 0) -
                                       (BindDown("key.left", 'A') ? 1 : 0));
        in.jump = BindDown("key.jump", VK_SPACE);
        in.sneak = BindDown("key.sneak", VK_SHIFT);
        in.sprint = BindDown("key.sprint", VK_CONTROL);
        g_player.flying = g_flying;
        tickPlayer(g_player, g_world, in);
        tickCombat(g_combat);
        g_mobs.tick(kFixedTick, g_player.pos, g_combat, in.sneak);
        if (g_combat.dead) {
            // Vanilla respawn resets health/hunger and returns to spawn.
            g_combat = CombatState{};
            spawnPlayer();
        }
        if (g_toastTimer > 0.0f) g_toastTimer -= kFixedTick;
        const int advNow = g_achievements.grantedCount();
        if (advNow != g_advCount) {
            g_advCount = advNow;
            const auto& all = g_achievements.all();
            for (auto it = all.rbegin(); it != all.rend(); ++it) {
                if (it->achieved) {
                    g_toastTitle = it->title;
                    g_toastDesc = it->description;
                    g_toastTimer = 5.0f;
                    g_sound.play("minecraft/sounds/random/levelup.ogg", 0.5f * g_volume);
                    break;
                }
            }
        }
        g_tickAccum -= kFixedTick;
    }
}

static void renderWorld(bool interpolate) {
    float alpha = 1.0f;
    if (interpolate) alpha = std::clamp(g_tickAccum / kFixedTick, 0.0f, 1.0f);
    Vec3 p{
        g_player.prevPos.x + (g_player.pos.x - g_player.prevPos.x) * alpha,
        g_player.prevPos.y + (g_player.pos.y - g_player.prevPos.y) * alpha,
        g_player.prevPos.z + (g_player.pos.z - g_player.prevPos.z) * alpha};
    Vec3 eye{p.x, p.y + Player::kEye, p.z};
    Vec3 forward = g_player.forward();
    if (g_thirdPerson && !g_debugCam) {
        eye = eye - forward * 4.0f;
    }
    if (g_debugCam) {
        eye = Vec3{128.0f, 150.0f, 128.0f};
        forward = normalize(Vec3{0.02f, -1.0f, 0.02f});
    }

    const float aspect = static_cast<float>(g_renderer.width()) /
                         static_cast<float>(std::max(1, g_renderer.height()));
    const float farPlane = std::max(96.0f, static_cast<float>(g_renderDistance) * 16.0f + 32.0f);
    const Mat4 view = Mat4::lookAt(eye, eye + forward, Vec3{0, 1, 0});
    const Mat4 proj = Mat4::perspective(g_fovDeg * kPi / 180.0f, aspect, 0.05f, farPlane);
    const Mat4 viewProj = view * proj;

    g_renderer.setFog(static_cast<float>(g_renderDistance) * 16.0f * 0.65f,
                      static_cast<float>(g_renderDistance) * 16.0f);
    const RenderStats rs = g_renderer.render(viewProj, eye, g_chunks, false);

    // Mobs: coloured boxes (entity models/textures come in a later batch).
    std::vector<EntityBox> boxes;
    boxes.reserve(g_mobs.mobs.size() + 2);
    for (const Mob& m : g_mobs.mobs) {
        if (m.state == MobState::Dead) continue;
        uint32_t color = 0xFF808080;
        switch (m.type) {
            case MobType::Pig: color = 0xFFEEBBAA; break;
            case MobType::Cow: color = 0xFF4A3320; break;
            case MobType::Sheep: color = 0xFFE0E0E0; break;
            case MobType::Chicken: color = 0xFFFFFFE0; break;
            case MobType::Zombie: color = 0xFF3F7A3F; break;
            case MobType::Skeleton: color = 0xFFC0C0C0; break;
            case MobType::Spider: color = 0xFF402020; break;
            case MobType::Creeper: color = 0xFF4CA04C; break;
            case MobType::Enderman: color = 0xFF181818; break;
            case MobType::EnderDragon: color = 0xFF301050; break;
            default: break;
        }
        boxes.push_back({m.pos, Vec3{m.width, m.height, m.width}, color});
    }
    if (g_thirdPerson) {
        // Placeholder player model until the vanilla model renderer lands.
        boxes.push_back({Vec3{p.x, p.y + 0.55f, p.z}, Vec3{0.6f, 1.1f, 0.35f}, 0xFF00A8A8});
        boxes.push_back({Vec3{p.x, p.y + 1.55f, p.z}, Vec3{0.5f, 0.5f, 0.5f}, 0xFFC79B6B});
    }
    g_renderer.renderEntities(boxes);
    static float logAccum = 0.0f;
    logAccum += 1.0f / 60.0f;
    if (g_debug && logAccum > 1.0f) {
        logAccum = 0.0f;
        logLine("render drawn=%d/%d tris=%d eye=(%.1f,%.1f,%.1f) fwd=(%.2f,%.2f,%.2f)",
                rs.drawn, rs.total, rs.triangles, eye.x, eye.y, eye.z,
                forward.x, forward.y, forward.z);
    }
}

// ---------------------------------------------------------------------------
// Loading screen
// ---------------------------------------------------------------------------
static void drawLoading() {
    g_gui.begin(g_guiW, g_guiH);
    drawDirtBackground();

    const int mojang = GetTex("minecraft/textures/gui/title/mojangstudios.png");
    if (mojang >= 0) {
        g_gui.quad(mojang, g_guiW * 0.5f - 64.0f, g_guiH * 0.5f - 96.0f, 128, 128,
                   0, 0, 1, 1, 0xFFFFFFFF);
    }

    const float bx = g_guiW * 0.5f - 100.0f;
    const float by = static_cast<float>(g_guiH) - 42.0f;
    g_gui.rect(bx - 1.0f, by - 1.0f, 202.0f, 12.0f, 0xFFFFFFFF);
    g_gui.rect(bx, by, 200.0f, 10.0f, 0xFF303030);
    g_gui.rect(bx, by, 200.0f * g_loadProgress, 10.0f, 0xFFFFFFFF);

    if (g_font.loaded()) {
        const float tw = g_font.measure(g_loadText, 1.0f);
        g_font.draw(g_gui, g_guiW * 0.5f - tw * 0.5f, by - 14.0f, g_loadText,
                    0xFFFFFFFF, true, 1.0f);
        char pct[32];
        snprintf(pct, sizeof(pct), "%d%%", static_cast<int>(g_loadProgress * 100.0f));
        const float pw = g_font.measure(pct, 1.0f);
        g_font.draw(g_gui, g_guiW * 0.5f - pw * 0.5f, by + 12.0f, pct, 0xFFFFFFFF, true, 1.0f);
    }
    g_gui.end();
}

static void failLoading(const std::string& err) {
    g_loadError = err;
    MessageBoxA(g_hwnd, err.c_str(), "CraftEngine - asset error", MB_ICONERROR);
    g_running = false;
}

static void runLoadStep() {
    logLine("load step %d (%s)", g_loadStep, g_loadText.c_str());
    switch (g_loadStep) {
        case 0:
            g_loadText = "Loading Minecraft 26.3";
            break;
        case 1: {
            g_loadText = "Loading assets";
            std::string err;
            if (!g_assets.init(err)) { failLoading(err); return; }
            std::string verr;
            if (!g_vanilla.load(ExeDir(), verr)) {
                logLine("vanilla data unavailable: %s", verr.c_str());
            } else {
                logLine("vanilla data: %s, %d registries, %d entries, %d keybinds",
                        g_vanilla.version().c_str(),
                        static_cast<int>(g_vanilla.registryCount()),
                        static_cast<int>(g_vanilla.totalEntries()),
                        static_cast<int>(g_vanilla.bindCount()));
            }
            break;
        }
        case 2:
            g_loadText = "Loading font";
            g_font.load(g_assets, g_gui);
            break;
        case 3: {
            g_loadText = "Loading block textures";
            std::vector<LoadedTexture> loaded;
            LoadTexturesFromEntries(g_assets.entries(), loaded);
            g_renderer.loadTextures(loaded);
            break;
        }
        case 4:
            g_loadText = "Starting sound engine";
            g_sound.init(&g_assets);
            break;
        case 5:
            g_loadText = "Loading panorama";
            g_panorama.load(g_renderer.device(), g_gui, g_assets);
            break;
        case 6:
            g_loadText = "Building terrain";
            g_world.generate(1337u);
            break;
        case 7:
            g_loadText = "Meshing chunks";
            g_world.buildMeshes(g_meshes);
            break;
        case 8:
            g_loadText = "Uploading chunks";
            g_chunks.assign(g_meshes.size(), GpuChunk{});
            for (int cz = 0; cz < World::CZ; ++cz) {
                for (int cx = 0; cx < World::CX; ++cx) {
                    const size_t i = static_cast<size_t>(cz) * World::CX + cx;
                    const Vec3 mn(static_cast<float>(cx * Chunk::SX), 0.0f,
                                  static_cast<float>(cz * Chunk::SZ));
                    const Vec3 mx(static_cast<float>((cx + 1) * Chunk::SX),
                                  static_cast<float>(Chunk::SY),
                                  static_cast<float>((cz + 1) * Chunk::SZ));
                    g_renderer.uploadChunk(g_chunks[i], g_meshes[i], mn, mx);
                }
            }
            break;
        case 9:
            g_loadText = "Preparing spawn area";
            spawnPlayer();
            break;
        default:
            g_state = State::Title;
            logLine("loading finished -> title");
            setCapture(false);
            g_panoramaYaw = 0.0f;
            if (!g_assets.splashes().empty()) {
                const auto& s = g_assets.splashes();
                g_splash = s[static_cast<size_t>(std::rand()) % s.size()];
            } else {
                g_splash = "Now with 100% more polygons!";
            }
            break;
    }
    ++g_loadStep;
    g_loadProgress = std::min(1.0f, static_cast<float>(g_loadStep) / 10.0f);
}

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
static void drawTitle(float dt) {
    g_panoramaYaw += dt * 0.012f;
    const float aspect = static_cast<float>(g_renderer.width()) /
                         static_cast<float>(std::max(1, g_renderer.height()));
    const float tanHalf = std::tan(g_fovDeg * kPi / 180.0f * 0.5f);
    g_panorama.draw(g_renderer.device(), g_gui, g_renderer.width(), g_renderer.height(),
                    g_panoramaYaw, 0.0f, tanHalf, aspect);

    g_gui.begin(g_guiW, g_guiH);

    const float logoX = g_guiW * 0.5f - 128.0f;
    const float logoY = 30.0f;
    const int logo = GetTex("minecraft/textures/gui/title/minecraft.png");
    if (logo >= 0) g_gui.quad(logo, logoX, logoY, 256, 64, 0, 0, 1, 1, 0xFFFFFFFF);
    const int edition = GetTex("minecraft/textures/gui/title/edition.png");
    if (edition >= 0) g_gui.quad(edition, logoX + 88.0f, logoY + 37.0f, 128, 16, 0, 0, 1, 1, 0xFFFFFFFF);

    if (!g_splash.empty() && g_font.loaded()) {
        const float t = static_cast<float>(std::fmod(g_time, 1.0));
        float scale = 1.8f - std::fabs(std::sin(t * kPi * 2.0f) * 0.1f);
        scale *= 100.0f / (g_font.measure(g_splash, 1.0f) + 32.0f);
        g_font.drawRot(g_gui, g_guiW * 0.5f + 90.0f, 70.0f, g_splash,
                       0xFFFFFF00, true, scale, -0.349066f);
    }

    buildTitleUi();
    drawUi();

    if (g_font.loaded()) {
        g_font.draw(g_gui, 2.0f, g_guiH - 10.0f, "Minecraft 26.3", 0xFFFFFFFF, true, 1.0f);
        const std::string right = "CraftEngine 0.2 | assets (c) Mojang";
        const float tw = g_font.measure(right, 1.0f);
        g_font.draw(g_gui, g_guiW - tw - 2.0f, g_guiH - 10.0f, right, 0xFFFFFFFF, true, 1.0f);
    }
    g_gui.end();
}

// ---------------------------------------------------------------------------
// Options screen
// ---------------------------------------------------------------------------
static void drawOptions() {
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    g_renderer.clear(black);
    g_gui.begin(g_guiW, g_guiH);
    drawDirtBackground();
    g_gui.rect(0, 0, static_cast<float>(g_guiW), static_cast<float>(g_guiH), 0x66000000);

    if (g_font.loaded()) {
        const std::string title = Lang("options.title", "Options");
        const float tw = g_font.measure(title, 1.0f);
        g_font.draw(g_gui, g_guiW * 0.5f - tw * 0.5f, 18.0f, title, 0xFFFFFFFF, true, 1.0f);
    }

    buildOptionsUi();
    drawUi();
    g_gui.end();
}

// ---------------------------------------------------------------------------
// Pause screen
// ---------------------------------------------------------------------------
static void drawPause() {
    g_gui.begin(g_guiW, g_guiH);
    g_gui.rect(0, 0, static_cast<float>(g_guiW), static_cast<float>(g_guiH), 0x99000000);
    if (g_font.loaded()) {
        const std::string title = Lang("menu.game", "Game Menu");
        const float tw = g_font.measure(title, 1.0f);
        g_font.draw(g_gui, g_guiW * 0.5f - tw * 0.5f, 44.0f, title, 0xFFFFFFFF, true, 1.0f);
    }
    buildPauseUi();
    drawUi();
    g_gui.end();
}

// ---------------------------------------------------------------------------
// In-game HUD
// ---------------------------------------------------------------------------
static std::string blockIconPath(Block b) {
    switch (b) {
        case GRASS: return "minecraft/textures/block/grass_block_side.png";
        case DIRT: return "minecraft/textures/block/dirt.png";
        case STONE: return "minecraft/textures/block/stone.png";
        case SAND: return "minecraft/textures/block/sand.png";
        case LOG: return "minecraft/textures/block/oak_log.png";
        case LEAVES: return "minecraft/textures/block/oak_leaves.png";
        case PLANKS: return "minecraft/textures/block/oak_planks.png";
        case COBBLE: return "minecraft/textures/block/cobblestone.png";
        case BEDROCK: return "minecraft/textures/block/bedrock.png";
        default: return "minecraft/textures/block/stone.png";
    }
}

static const char* itemIconPath(uint16_t id) {
    switch (id) {
        case ITEM_GRASS_BLOCK: return "minecraft/textures/block/grass_block_side.png";
        case ITEM_DIRT: return "minecraft/textures/block/dirt.png";
        case ITEM_STONE: return "minecraft/textures/block/stone.png";
        case ITEM_COBBLESTONE: return "minecraft/textures/block/cobblestone.png";
        case ITEM_OAK_PLANKS: return "minecraft/textures/block/oak_planks.png";
        case ITEM_OAK_LOG: return "minecraft/textures/block/oak_log.png";
        case ITEM_OAK_LEAVES: return "minecraft/textures/block/oak_leaves.png";
        case ITEM_SAND: return "minecraft/textures/block/sand.png";
        case ITEM_BEDROCK: return "minecraft/textures/block/bedrock.png";
        case ITEM_WOODEN_PICKAXE: return "minecraft/textures/item/wooden_pickaxe.png";
        case ITEM_STONE_PICKAXE: return "minecraft/textures/item/stone_pickaxe.png";
        case ITEM_IRON_PICKAXE: return "minecraft/textures/item/iron_pickaxe.png";
        case ITEM_DIAMOND_PICKAXE: return "minecraft/textures/item/diamond_pickaxe.png";
        case ITEM_DIAMOND_SWORD: return "minecraft/textures/item/diamond_sword.png";
        case ITEM_TORCH: return "minecraft/textures/block/torch.png";
        case ITEM_BREAD: return "minecraft/textures/item/bread.png";
        case ITEM_COAL: return "minecraft/textures/item/coal.png";
        case ITEM_DIAMOND: return "minecraft/textures/item/diamond.png";
        case ITEM_ARMOR_CHEST: return "minecraft/textures/item/diamond_chestplate.png";
        default: return "minecraft/textures/item/stick.png";
    }
}

static void drawHud() {
    g_gui.begin(g_guiW, g_guiH);

    if (!g_hudHidden) {
        const int cross = GetTex("minecraft/textures/gui/sprites/hud/crosshair.png");
        if (cross >= 0) {
            g_gui.quad(cross, g_guiW * 0.5f - 7.5f, g_guiH * 0.5f - 7.5f, 15, 15,
                       0, 0, 1, 1, 0xFFFFFFFF);
        }

        const int hotbar = GetTex("minecraft/textures/gui/sprites/hud/hotbar.png");
        const float hx = g_guiW * 0.5f - 91.0f;
        const float hy = static_cast<float>(g_guiH) - 22.0f;
        if (hotbar >= 0) g_gui.quad(hotbar, hx, hy, 182, 22, 0, 0, 1, 1, 0xFFFFFFFF);

        for (int i = 0; i < 9; ++i) {
            const ItemStack& s = g_inventory.hotbar(i);
            if (s.empty()) continue;
            const int icon = GetTex(itemIconPath(s.id));
            if (icon >= 0) {
                g_gui.quad(icon, hx + 3.0f + i * 20.0f, hy + 3.0f, 16, 16,
                           0, 0, 1, 1, 0xFFFFFFFF);
            }
            if (s.count > 1 && g_font.loaded()) {
                char cnt[8];
                snprintf(cnt, sizeof(cnt), "%d", s.count);
                g_font.draw(g_gui, hx + 3.0f + i * 20.0f + 16.0f - g_font.measure(cnt, 0.8f),
                            hy + 3.0f + 8.0f, cnt, 0xFFFFFFFF, true, 0.8f);
            }
        }

        const int sel = GetTex("minecraft/textures/gui/sprites/hud/hotbar_selection.png");
        if (sel >= 0) {
            g_gui.quad(sel, hx - 1.0f + g_inventory.selected * 20.0f, hy - 1.0f, 24, 23,
                       0, 0, 1, 1, 0xFFFFFFFF);
        }

        // Hearts + hunger (vanilla: 10 hearts, 10 drumsticks).
        for (int i = 0; i < 10; ++i) {
            const float hpx = g_guiW * 0.5f - 91.0f + i * 18.0f;
            const float hpy = static_cast<float>(g_guiH) - 37.0f;
            g_gui.rect(hpx, hpy, 9, 9, 0xFF000000);
            const float hp = std::clamp(g_combat.health - i * 2.0f, 0.0f, 2.0f) * 0.5f;
            g_gui.rect(hpx, hpy, 9.0f * hp, 9, 0xFFFF2020);
            g_gui.rect(hpx + 93.0f, hpy, 9, 9, 0xFF000000);
            g_gui.rect(hpx + 93.0f, hpy + (1.0f - std::clamp(g_combat.health / 20.0f, 0.0f, 1.0f)) * 9.0f,
                       9, 9.0f * std::clamp(g_combat.health / 20.0f, 0.0f, 1.0f), 0xFFFFA020);
        }

        // Selected item name above the hotbar, as in vanilla.
        const ItemStack& held = g_inventory.selectedStack();
        if (!held.empty() && g_font.loaded()) {
            const std::string name = itemName(held.id);
            const float tw = g_font.measure(name, 1.0f);
            g_font.draw(g_gui, g_guiW * 0.5f - tw * 0.5f,
                        static_cast<float>(g_guiH) - 42.0f, name, 0xFFFFFFFF, true, 1.0f);
        }
    }

    // Achievement toast (vanilla style: title + description, slides in/out).
    if (g_toastTimer > 0.0f && g_font.loaded()) {
        const float alphaT = std::clamp(g_toastTimer, 0.0f, 1.0f);
        const uint32_t alpha = static_cast<uint32_t>(alphaT * 255.0f) << 24;
        g_gui.rect(g_guiW - 172.0f, 4.0f, 168.0f, 34.0f, alpha | 0x00000000u);
        g_font.draw(g_gui, g_guiW - 168.0f, 8.0f, g_toastTitle, 0xFFFFFF55 | alpha, true, 1.0f);
        g_font.draw(g_gui, g_guiW - 168.0f, 20.0f, g_toastDesc, 0xFFFFFFFF | alpha, true, 0.8f);
    }

    if (g_debug && g_font.loaded()) {
        char line[128];
        g_font.draw(g_gui, 2, 2, "Minecraft 26.3 (CraftEngine C++/DX10)", 0xFFFFFFFF, true, 1.0f);
        snprintf(line, sizeof(line), "FPS: %.0f", g_fps);
        g_font.draw(g_gui, 2, 12, line, 0xFFFFFFFF, true, 1.0f);
        snprintf(line, sizeof(line), "XYZ: %.3f / %.3f / %.3f",
                 g_player.pos.x, g_player.pos.y, g_player.pos.z);
        g_font.draw(g_gui, 2, 22, line, 0xFFFFFFFF, true, 1.0f);
        snprintf(line, sizeof(line), "Chunk: %d %d  Slot: %d",
                 static_cast<int>(std::floor(g_player.pos.x)) >> 4,
                 static_cast<int>(std::floor(g_player.pos.z)) >> 4,
                 g_inventory.selected + 1);
        g_font.draw(g_gui, 2, 32, line, 0xFFFFFFFF, true, 1.0f);
        const float yawDeg = std::fmod(g_player.yaw * 180.0f / kPi + 720.0f, 360.0f);
        const char* dir = "north";
        if (yawDeg >= 45 && yawDeg < 135) dir = "east";
        else if (yawDeg >= 135 && yawDeg < 225) dir = "south";
        else if (yawDeg >= 225 && yawDeg < 315) dir = "west";
        snprintf(line, sizeof(line), "Facing: %s (%.1f)", dir, yawDeg);
        g_font.draw(g_gui, 2, 42, line, 0xFFFFFFFF, true, 1.0f);
    }

    g_gui.end();
}

// ---------------------------------------------------------------------------
// Block interaction
// ---------------------------------------------------------------------------
static std::string breakSound(Block b) {
    switch (b) {
        case GRASS:
        case LEAVES: return "minecraft/sounds/dig/grass1.ogg";
        case SAND: return "minecraft/sounds/dig/sand1.ogg";
        case DIRT: return "minecraft/sounds/dig/gravel1.ogg";
        case LOG:
        case PLANKS: return "minecraft/sounds/dig/wood1.ogg";
        default: return "minecraft/sounds/dig/stone1.ogg";
    }
}

static bool blockHitsPlayer(int x, int y, int z) {
    const float px = g_player.pos.x, py = g_player.pos.y, pz = g_player.pos.z;
    return (static_cast<float>(x) + 1.0f > px - Player::kHalfW &&
            static_cast<float>(x) < px + Player::kHalfW &&
            static_cast<float>(y) + 1.0f > py && static_cast<float>(y) < py + Player::kHeight &&
            static_cast<float>(z) + 1.0f > pz - Player::kHalfW &&
            static_cast<float>(z) < pz + Player::kHalfW);
}

static uint16_t blockToItem(Block b) {
    switch (b) {
        case GRASS: return ITEM_GRASS_BLOCK;
        case DIRT: return ITEM_DIRT;
        case STONE: return ITEM_STONE;
        case COBBLE: return ITEM_COBBLESTONE;
        case PLANKS: return ITEM_OAK_PLANKS;
        case LOG: return ITEM_OAK_LOG;
        case LEAVES: return ITEM_OAK_LEAVES;
        case SAND: return ITEM_SAND;
        case BEDROCK: return ITEM_BEDROCK;
        default: return ITEM_STONE;
    }
}

static Block itemToBlock(uint16_t id) {
    switch (id) {
        case ITEM_GRASS_BLOCK: return GRASS;
        case ITEM_DIRT: return DIRT;
        case ITEM_STONE: return STONE;
        case ITEM_COBBLESTONE: return COBBLE;
        case ITEM_OAK_PLANKS: return PLANKS;
        case ITEM_OAK_LOG: return LOG;
        case ITEM_OAK_LEAVES: return LEAVES;
        case ITEM_SAND: return SAND;
        case ITEM_BEDROCK: return BEDROCK;
        default: return AIR;
    }
}

static void breakBlock() {
    const Vec3 eye = g_player.eye();
    const float reach = g_creative ? Vanilla::REACH_CREATIVE : Vanilla::REACH_SURVIVAL;
    const RayHit hit = raycast(g_world, eye, g_player.forward(), reach);
    if (!hit.hit) return;
    const Block b = g_world.getBlock(hit.x, hit.y, hit.z);
    if (b == AIR || b == BEDROCK) return;    g_world.setBlock(hit.x, hit.y, hit.z, AIR);
    remeshAround(hit.x, hit.z);
    g_sound.play(breakSound(b), 0.7f * g_volume);
    g_achievements.onBlockBroken(static_cast<uint8_t>(b));
    g_inventory.add({blockToItem(b), 1, 0});
}

static void attackMob() {
    const Vec3 eye = g_player.eye();
    const int idx = g_mobs.pick(eye, g_player.forward(), Vanilla::REACH_SURVIVAL);
    if (idx < 0) return;

    CombatState dummy; // attacker state is g_combat; target temporarily copied
    const Mob& m = g_mobs.mobs[static_cast<size_t>(idx)];
    CombatState target;
    target.health = m.health;
    target.invulnerableTicks = m.hurtTime;

    int armor = 0;
    float toughness = 0.0f;
    armorFromInventory(g_inventory, armor, toughness);
    (void)armor; (void)toughness;

    const float dealt = playerAttack(g_combat, g_inventory, target,
                                     BindDown("key.sprint", VK_CONTROL), g_player.onGround);
    if (dealt < 0.0f) return; // cooldown

    g_mobs.damage(idx, dealt, DamageSource::Player);
    g_mobs.mobs[static_cast<size_t>(idx)].vel += knockbackVelocity(
        g_player.pos, m.pos, 1.0f);
    g_inventory.damageItem(static_cast<int>(g_inventory.selected), 1);
    g_sound.play("minecraft/sounds/random/click.ogg", 0.5f * g_volume);

    if (m.health - dealt <= 0.0f) {
        g_achievements.onMobKilled(static_cast<uint8_t>(m.type));
    }
}

static bool attackMobTry() {
    const Vec3 eye = g_player.eye();
    if (g_mobs.pick(eye, g_player.forward(), Vanilla::REACH_SURVIVAL) < 0) return false;
    attackMob();
    return true;
}

static void placeBlock() {
    const Vec3 eye = g_player.eye();
    const float reach = g_creative ? Vanilla::REACH_CREATIVE : Vanilla::REACH_SURVIVAL;
    const RayHit hit = raycast(g_world, eye, g_player.forward(), reach);
    if (!hit.hit) return;
    const int x = hit.x + hit.nx;
    const int y = hit.y + hit.ny;
    const int z = hit.z + hit.nz;
    if (g_world.getBlock(x, y, z) != AIR) return;
    if (blockHitsPlayer(x, y, z)) return;
    const Block b = itemToBlock(g_inventory.selectedStack().id);
    if (b == AIR) return;
    g_world.setBlock(x, y, z, b);
    remeshAround(x, z);
    g_sound.play(breakSound(b), 0.45f * g_volume);
    if (!g_inventory.selectedStack().empty()) {
        --g_inventory.selectedStack().count;
        if (g_inventory.selectedStack().count == 0) g_inventory.selectedStack().clear();
    }
}

// Player.drop(boolean): one item leaves the selected stack.
static void dropSelected() {
    ItemStack& stack = g_inventory.selectedStack();
    if (stack.empty()) return;
    --stack.count;
    if (stack.count == 0) stack.clear();
}

// Player.swapOffhand(): selected hotbar slot <-> offhand slot.
static void swapOffhand() {
    std::swap(g_inventory.selectedStack(), g_inventory.offhand());
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
            g_running = false;
            return 0;
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (g_renderer.device() && wp != SIZE_MINIMIZED)
                g_renderer.resize(LOWORD(lp), HIWORD(lp));
            return 0;
        case WM_MOUSEMOVE:
            g_mouseX = GET_X_LPARAM(lp);
            g_mouseY = GET_Y_LPARAM(lp);
            if (g_activeSlider >= 0 && (GetKeyState(VK_LBUTTON) & 0x8000))
                applySlider(g_activeSlider, mouseGuiX());
            return 0;
        case WM_LBUTTONUP:
            g_activeSlider = -1;
            return 0;
        case WM_LBUTTONDOWN:
            g_mouseX = GET_X_LPARAM(lp);
            g_mouseY = GET_Y_LPARAM(lp);
            if (g_state == State::InGame && g_captured) {
                if (!attackMobTry()) breakBlock();
            } else if (g_state != State::InGame) handleUiClick(mouseGuiX(), mouseGuiY());
            return 0;
        case WM_RBUTTONDOWN:
            if (g_state == State::InGame && g_captured) placeBlock();
            return 0;
        case WM_MOUSEWHEEL:
            if (g_state == State::InGame && g_captured) {
                const int delta = GET_WHEEL_DELTA_WPARAM(wp);
                if (delta > 0) g_inventory.selected = (g_inventory.selected + 8) % 9;
                else if (delta < 0) g_inventory.selected = (g_inventory.selected + 1) % 9;
            }
            return 0;
        case WM_KEYDOWN: {
            const int vk = static_cast<int>(wp & 0xFF);
            const bool autoRepeat = (lp & (1 << 30)) != 0;
            g_keys[vk] = true;
            if (wp == VK_ESCAPE) {
                if (g_state == State::InGame) {
                    if (g_inventoryOpen) {
                        g_inventoryOpen = false;
                        setCapture(true);
                    } else {
                        pauseGame();
                    }
                } else if (g_state == State::Pause) {
                    resumeGame();
                } else if (g_state == State::Options) {
                    saveOptions();
                    g_state = g_optionsReturn;
                    if (g_state == State::InGame) setCapture(true);
                }
                return 0;
            }
            if (g_state == State::InGame) {
                for (int i = 0; i < 9; ++i) {
                    char bind[24];
                    snprintf(bind, sizeof(bind), "key.hotbar.%d", i + 1);
                    if (wp == static_cast<WPARAM>(BindVK(bind, '1' + i))) {
                        g_inventory.selected = static_cast<uint16_t>(i);
                        return 0;
                    }
                }
                if (wp == static_cast<WPARAM>(BindVK("key.inventory", 'E'))) {
                    g_inventoryOpen = !g_inventoryOpen;
                    setCapture(!g_inventoryOpen);
                } else if (wp == static_cast<WPARAM>(BindVK("key.drop", 'Q'))) {
                    dropSelected();
                } else if (wp == static_cast<WPARAM>(BindVK("key.swapOffhand", 'F'))) {
                    swapOffhand();
                } else if (wp == static_cast<WPARAM>(BindVK("key.toggleGui", VK_F1))) {
                    g_hudHidden = !g_hudHidden;
                } else if (wp == static_cast<WPARAM>(BindVK("key.debug.overlay", VK_F3))) {
                    g_debug = !g_debug;
                } else if (wp == static_cast<WPARAM>(BindVK("key.fullscreen", VK_F11))) {
                    toggleFullscreen();
                } else if (wp == static_cast<WPARAM>(BindVK("key.togglePerspective", VK_F5))) {
                    g_thirdPerson = !g_thirdPerson;
                    logLine("perspective: %s", g_thirdPerson ? "third" : "first");
                } else if (wp == static_cast<WPARAM>(BindVK("key.jump", VK_SPACE)) &&
                           g_creative && !g_inventoryOpen && !autoRepeat) {
                    // Vanilla CreativeMode: double-tap jump toggles flight.
                    const uint64_t now = GetTickCount64();
                    if (now - g_lastJumpPress <= 300) {
                        g_flying = !g_flying;
                        g_player.flying = g_flying;
                    }
                    g_lastJumpPress = now;
                }
            }
            return 0;
        }
        case WM_KEYUP:
            g_keys[wp & 0xFF] = false;
            return 0;
        case WM_KILLFOCUS:
            std::fill(std::begin(g_keys), std::end(g_keys), false);
            if (g_state == State::InGame) pauseGame();
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    loadOptions();
    logLine("CraftEngine start, exe dir %s", ExeDir().c_str());

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"CraftEngineWindow";
    if (!RegisterClassExW(&wc)) return 1;

    RECT rc{0, 0, kStartWidth, kStartHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Minecraft 26.3",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top,
                             nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    if (!g_renderer.init(g_hwnd, kStartWidth, kStartHeight)) {
        MessageBoxW(g_hwnd, L"Direct3D 10 device creation failed.\n"
                            L"Your GPU may not support DX10.",
                    L"CraftEngine", MB_ICONERROR);
        return 1;
    }
    if (!g_gui.init(g_renderer.device())) {
        MessageBoxW(g_hwnd, L"GUI initialization failed.", L"CraftEngine", MB_ICONERROR);
        return 1;
    }

    LARGE_INTEGER freq{};
    QueryPerformanceFrequency(&freq);
    LARGE_INTEGER prev{};
    QueryPerformanceCounter(&prev);

    MSG msg{};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) break;

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        float dt = static_cast<float>(now.QuadPart - prev.QuadPart) /
                   static_cast<float>(freq.QuadPart);
        prev = now;
        dt = std::min(dt, 0.1f);
        g_time += dt;
        g_sound.update();

        g_fpsAccum += dt;
        ++g_fpsFrames;
        if (g_fpsAccum >= 0.5f) {
            g_fps = static_cast<float>(g_fpsFrames) / g_fpsAccum;
            g_fpsAccum = 0.0f;
            g_fpsFrames = 0;
            wchar_t title[128];
            swprintf(title, 128, L"Minecraft 26.3  |  %.0f FPS", g_fps);
            SetWindowTextW(g_hwnd, title);
        }

        computeGuiDims(g_renderer.width(), g_renderer.height());

        if (g_state == State::Loading) {
            const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            g_renderer.clear(black);
            drawLoading();
            g_renderer.present(g_vsync);
            runLoadStep();
            continue;
        }

        if (g_state == State::InGame) {
            updateMouseLook();
            tickGame(dt);
            renderWorld(true);
            drawHud();
        } else if (g_state == State::Title) {
            const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            g_renderer.clear(black);
            drawTitle(dt);
        } else if (g_state == State::Options) {
            drawOptions();
        } else if (g_state == State::Pause) {
            renderWorld(false);
            drawPause();
        }

        g_renderer.present(g_vsync);
    }

    g_panorama.shutdown(g_renderer.device());
    g_sound.shutdown();
    g_gui.shutdown();
    g_renderer.shutdown();
    return 0;
}
