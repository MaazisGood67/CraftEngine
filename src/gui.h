// CraftEngine - 2D GUI renderer (sprite/text batching) + panorama cubemap.
#pragma once
#include <cstdint>
#include <vector>

#include "assets.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d10.h>

class Gui {
public:
    bool init(ID3D10Device* device);
    void shutdown();

    void begin(int screenW, int screenH);
    void end();

    int createTexture(const uint8_t* rgba, int w, int h);
    ID3D10ShaderResourceView* srv(int tex) const;

    // Pixel-space draws (caller applies GUI scale).
    void quad(int tex, float x, float y, float w, float h,
              float u0, float v0, float u1, float v1, uint32_t argb);
    void quadRot(int tex, float cx, float cy, float w, float h, float angleRad,
                 float u0, float v0, float u1, float v1, uint32_t argb);
    void rect(float x, float y, float w, float h, uint32_t argb);

private:
    struct Vert {
        float x, y, u, v;
        float r, g, b, a;
    };

    void flush();

    ID3D10Device* device_ = nullptr;
    ID3D10VertexShader* vs_ = nullptr;
    ID3D10PixelShader* ps_ = nullptr;
    ID3D10InputLayout* layout_ = nullptr;
    ID3D10Buffer* cb_ = nullptr;
    ID3D10Buffer* vb_ = nullptr;
    ID3D10Buffer* ib_ = nullptr;
    ID3D10SamplerState* sampler_ = nullptr;
    ID3D10BlendState* blend_ = nullptr;
    ID3D10RasterizerState* raster_ = nullptr;
    ID3D10DepthStencilState* depthOff_ = nullptr;

    std::vector<ID3D10ShaderResourceView*> textures_;
    std::vector<Vert> verts_;
    int currentTex_ = -1;
    int screenW_ = 0;
    int screenH_ = 0;
};

// Vanilla title-screen panorama (6 cube faces rendered by a ray-mapped PS).
class Panorama {
public:
    bool load(ID3D10Device* device, Gui& gui, const Assets& assets);
    void draw(ID3D10Device* device, Gui& gui, int screenW, int screenH,
              float yawRad, float pitchRad, float tanHalfFov, float aspect);
    void shutdown(ID3D10Device* device);

private:
    ID3D10VertexShader* vs_ = nullptr;
    ID3D10PixelShader* ps_ = nullptr;
    ID3D10Buffer* cb_ = nullptr;
    ID3D10DepthStencilState* depthOff_ = nullptr;
    ID3D10SamplerState* sampler_ = nullptr;
    int faces_[6] = {-1, -1, -1, -1, -1, -1};
    bool ready_ = false;
};

// Loads a PNG asset into a GUI texture; returns -1 when missing/undecodable.
int LoadPngTexture(Gui& gui, const Assets& assets, const char* name, int* outW = nullptr, int* outH = nullptr);
