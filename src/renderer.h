// Direct3D 10 renderer (SM4.0) tuned for DX10-class GPUs such as Intel Ironlake.
#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d10.h>
#include <string>
#include <vector>

#include "chunk.h"
#include "vmath.h"
#include "textures.h"

struct Frustum {
    float planes[6][4]{};
    void fromViewProj(const Mat4& vp);
    bool testAABB(const Vec3& mn, const Vec3& mx) const;
};

struct GpuChunk {
    ID3D10Buffer* vb = nullptr;
    ID3D10Buffer* ib = nullptr;
    UINT indexCount = 0;
    Vec3 min;
    Vec3 max;
};

struct RenderStats {
    int drawn = 0;
    int total = 0;
    unsigned long long triangles = 0;
};

class Renderer {
public:
    bool init(HWND hwnd, int width, int height);
    void shutdown();

    bool resize(int width, int height);

    bool loadTextures(const std::vector<LoadedTexture>& loaded);

    void uploadChunk(GpuChunk& gpu, const ChunkMesh& mesh, const Vec3& mn, const Vec3& mx);
    RenderStats render(const Mat4& viewProj, const Vec3& camPos,
                       const std::vector<GpuChunk>& chunks,
                       bool wireframe, bool vsync);

    ID3D10Device* device() const { return device_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    bool createSwapChain(HWND hwnd, int width, int height);
    bool createTargets();
    void releaseTargets();
    bool createPipelines();
    bool createTextureArray();

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    IDXGISwapChain* swap_ = nullptr;
    ID3D10Device* device_ = nullptr;
    ID3D10RenderTargetView* rtv_ = nullptr;
    ID3D10Texture2D* depthTex_ = nullptr;
    ID3D10DepthStencilView* dsv_ = nullptr;

    ID3D10InputLayout* inputLayout_ = nullptr;
    ID3D10VertexShader* vs_ = nullptr;
    ID3D10PixelShader* ps_ = nullptr;
    ID3D10Buffer* cb_ = nullptr;
    ID3D10SamplerState* sampler_ = nullptr;
    ID3D10ShaderResourceView* texArray_ = nullptr;
    ID3D10RasterizerState* rasterSolid_ = nullptr;
    ID3D10RasterizerState* rasterWire_ = nullptr;
    ID3D10DepthStencilState* depthState_ = nullptr;
    ID3D10BlendState* blendState_ = nullptr;

    std::vector<uint8_t> tilePixels_;
    std::vector<LoadedTexture> loaded_;
    bool texturesLoaded_ = false;

    struct Constants {
        float viewProj[16];
        float camPos[4];
        float fogColor[4];
        float fogParams[4];
    };
};
