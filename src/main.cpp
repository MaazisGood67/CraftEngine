// CraftEngine - native DX10 voxel engine entry point.
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "renderer.h"
#include "textures.h"
#include "world.h"
#include "zip.h"

namespace {

constexpr int kStartWidth = 1280;
constexpr int kStartHeight = 720;
constexpr float kFovY = 1.2217f; // 70 degrees
constexpr float kMouseSensitivity = 0.0022f;

struct Camera {
    Vec3 pos{128.0f, 60.0f, 128.0f};
    float yaw = 0.0f;
    float pitch = -0.15f;

    Vec3 forward() const {
        return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    }
    Vec3 right() const { return {std::cos(yaw), 0.0f, -std::sin(yaw)}; }
};

Renderer* g_renderer = nullptr;
Camera g_camera;
bool g_keys[256]{};
bool g_captured = false;
bool g_running = true;

void setCapture(bool on, HWND hwnd) {
    g_captured = on;
    if (on) {
        ShowCursor(FALSE);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        POINT c{(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
        ClientToScreen(hwnd, &c);
        SetCursorPos(c.x, c.y);
    } else {
        ShowCursor(TRUE);
    }
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (g_renderer && wp != SIZE_MINIMIZED)
                g_renderer->resize(LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_KEYDOWN:
            g_keys[wp & 0xFF] = true;
            if (wp == VK_ESCAPE) {
                if (g_captured) setCapture(false, hwnd);
                else g_running = false;
            }
            return 0;

        case WM_KEYUP:
            g_keys[wp & 0xFF] = false;
            return 0;

        case WM_LBUTTONDOWN:
            if (!g_captured) setCapture(true, hwnd);
            return 0;

        case WM_KILLFOCUS:
            std::fill(std::begin(g_keys), std::end(g_keys), false);
            if (g_captured) setCapture(false, hwnd);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();

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

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"CraftEngine",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    Renderer renderer;
    if (!renderer.init(hwnd, kStartWidth, kStartHeight)) {
        MessageBoxW(hwnd, L"Direct3D 10 device creation failed.\n"
                          L"Your GPU may not support DX10, or another application is using it.",
                    L"CraftEngine", MB_ICONERROR);
        return 1;
    }
    g_renderer = &renderer;

    // ---- load real textures from the user's Minecraft jar (fallback: procedural) ----
    {
        const wchar_t* jarWide = L"C:\\Users\\Administrator\\AppData\\Roaming\\Migurinth\\meta\\versions\\26.3\\26.3.jar";
        const int len = WideCharToMultiByte(CP_UTF8, 0, jarWide, -1, nullptr, 0, nullptr, nullptr);
        std::string jarPath(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, jarWide, -1, &jarPath[0], len, nullptr, nullptr);
        std::vector<LoadedTexture> loaded;
        std::string texErr;
        const bool okT = LoadTexturesFromJar(jarPath, loaded, texErr);
        if (okT) {
            renderer.loadTextures(loaded);
        }
    }

    // ---- world ----
    World world;
    world.generate(1337u);

    std::vector<ChunkMesh> meshes;
    world.buildMeshes(meshes);

    std::vector<GpuChunk> gpuChunks(meshes.size());
    unsigned long long totalTris = 0;
    for (int cz = 0; cz < World::CZ; ++cz) {
        for (int cx = 0; cx < World::CX; ++cx) {
            const size_t i = static_cast<size_t>(cz) * World::CX + cx;
            const Vec3 mn(static_cast<float>(cx * Chunk::SX), 0.0f, static_cast<float>(cz * Chunk::SZ));
            const Vec3 mx(mn.x + Chunk::SX, static_cast<float>(Chunk::SY), mn.z + Chunk::SZ);
            renderer.uploadChunk(gpuChunks[i], meshes[i], mn, mx);
            totalTris += meshes[i].indices.size() / 3;
        }
    }

    // Spawn on top of the terrain at the world centre.
    g_camera.pos.y = static_cast<float>(world.heightAt(128, 128)) + 6.0f;

    LARGE_INTEGER freq{};
    QueryPerformanceFrequency(&freq);
    LARGE_INTEGER prev{};
    QueryPerformanceCounter(&prev);

    double fpsAccum = 0.0;
    int fpsFrames = 0;
    float fpsValue = 0.0f;
    bool wireframe = false;
    bool vsync = true;

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
        float dt = static_cast<float>(now.QuadPart - prev.QuadPart) / static_cast<float>(freq.QuadPart);
        prev = now;
        dt = std::min(dt, 0.1f);

        // ---- input ----
        if (g_captured) {
            RECT client{};
            GetClientRect(hwnd, &client);
            POINT center{(client.right - client.left) / 2, (client.bottom - client.top) / 2};
            ClientToScreen(hwnd, &center);

            POINT cur{};
            GetCursorPos(&cur);
            const float dx = static_cast<float>(cur.x - center.x);
            const float dy = static_cast<float>(cur.y - center.y);
            if (dx != 0.0f || dy != 0.0f) {
                g_camera.yaw += dx * kMouseSensitivity;
                g_camera.pitch -= dy * kMouseSensitivity;
                g_camera.pitch = std::clamp(g_camera.pitch, -1.55f, 1.55f);
                SetCursorPos(center.x, center.y);
            }
        }

        const float speed = g_keys[VK_SHIFT] ? 34.0f : 13.0f;
        Vec3 move{};
        const Vec3 fwd = g_camera.forward();
        const Vec3 right = g_camera.right();
        if (g_keys['W']) move += fwd;
        if (g_keys['S']) move += fwd * -1.0f;
        if (g_keys['D']) move += right;
        if (g_keys['A']) move += right * -1.0f;
        if (g_keys[VK_SPACE]) move.y += 1.0f;
        if (g_keys[VK_CONTROL]) move.y -= 1.0f;
        if (length(move) > 0.0f) g_camera.pos += normalize(move) * (speed * dt);
        g_camera.pos.y = std::clamp(g_camera.pos.y, 1.5f, static_cast<float>(Chunk::SY) - 2.0f);

        if (g_keys[VK_F1]) {
            wireframe = !wireframe;
            g_keys[VK_F1] = false;
        }
        if (g_keys['V']) {
            vsync = !vsync;
            g_keys['V'] = false;
        }

        // ---- render ----
        const float aspect = static_cast<float>(renderer.width()) /
                             static_cast<float>(std::max(1, renderer.height()));
        const Mat4 view = Mat4::lookAt(g_camera.pos, g_camera.pos + fwd, Vec3{0, 1, 0});
        const Mat4 proj = Mat4::perspective(kFovY, aspect, 0.1f, 320.0f);
        const Mat4 viewProj = view * proj;

        const RenderStats stats = renderer.render(viewProj, g_camera.pos, gpuChunks, wireframe, vsync);

        // ---- stats ----
        fpsAccum += dt;
        ++fpsFrames;
        if (fpsAccum >= 0.5) {
            fpsValue = static_cast<float>(fpsFrames / fpsAccum);
            fpsAccum = 0.0;
            fpsFrames = 0;

            wchar_t title[256];
            swprintf_s(title,
                       L"CraftEngine v0.1  |  %.0f FPS  |  chunks %d/%d  |  %.2fM tris drawn  |  %.0f,%.0f,%.0f  |  %s%s",
                       fpsValue, stats.drawn, stats.total,
                       static_cast<double>(stats.triangles) / 1.0e6,
                       g_camera.pos.x, g_camera.pos.y, g_camera.pos.z,
                       vsync ? L"vsync" : L"novsync",
                       wireframe ? L"  wireframe" : L"");
            SetWindowTextW(hwnd, title);
        }
    }

    g_renderer = nullptr;
    renderer.shutdown();
    return 0;
}
