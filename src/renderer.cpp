#include "renderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "shaders.h"

namespace {

constexpr float kSkyColor[4] = {0.62f, 0.80f, 1.00f, 1.0f};

inline uint32_t hashp(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

inline float rnd(int x, int y, int tile) {
    return static_cast<float>(hashp(static_cast<uint32_t>(x) * 73856093u ^
                                    static_cast<uint32_t>(y) * 19349663u ^
                                    static_cast<uint32_t>(tile) * 83492791u) & 0xFFFFu) /
           65535.0f;
}

struct Color {
    float r, g, b;
};

inline uint8_t toByte(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void putPixel(uint8_t* dst, int x, int y, const Color& c) {
    const int o = (y * 16 + x) * 4;
    dst[o + 0] = toByte(c.r);
    dst[o + 1] = toByte(c.g);
    dst[o + 2] = toByte(c.b);
    dst[o + 3] = 255;
}

void shade(Color& c, float f) {
    c.r *= f;
    c.g *= f;
    c.b *= f;
}

// Procedural 16x16 tiles that match the classic look (nearest-neighbour sampled).
void generateTile(uint8_t* dst, int tile) {
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const float n = rnd(x, y, tile);
            Color c{0.5f, 0.5f, 0.5f};

            switch (tile) {
                case TILE_GRASS_TOP: {
                    c = {0.34f, 0.60f, 0.22f};
                    shade(c, 0.82f + n * 0.34f);
                    break;
                }
                case TILE_GRASS_SIDE: {
                    if (y < 3) {
                        c = {0.32f, 0.58f, 0.21f};
                        shade(c, 0.85f + n * 0.3f);
                    } else {
                        c = {0.47f, 0.33f, 0.22f};
                        shade(c, 0.82f + n * 0.34f);
                    }
                    break;
                }
                case TILE_DIRT: {
                    c = {0.47f, 0.33f, 0.22f};
                    shade(c, 0.82f + n * 0.34f);
                    break;
                }
                case TILE_STONE: {
                    c = {0.53f, 0.53f, 0.55f};
                    shade(c, 0.78f + n * 0.38f);
                    break;
                }
                case TILE_SAND: {
                    c = {0.85f, 0.81f, 0.60f};
                    shade(c, 0.88f + n * 0.22f);
                    break;
                }
                case TILE_WATER: {
                    c = {0.18f, 0.38f, 0.80f};
                    shade(c, 0.85f + n * 0.25f);
                    break;
                }
                case TILE_LOG_SIDE: {
                    const float streak = 0.85f + rnd(x, 0, 99) * 0.3f;
                    c = {0.42f, 0.30f, 0.17f};
                    shade(c, streak * (0.9f + n * 0.15f));
                    break;
                }
                case TILE_LOG_TOP: {
                    const float dx = static_cast<float>(x) - 7.5f;
                    const float dy = static_cast<float>(y) - 7.5f;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const bool ring = (static_cast<int>(d) % 2) == 0;
                    c = ring ? Color{0.50f, 0.37f, 0.22f} : Color{0.40f, 0.28f, 0.16f};
                    shade(c, 0.9f + n * 0.15f);
                    break;
                }
                case TILE_LEAVES: {
                    c = {0.20f, 0.46f, 0.16f};
                    shade(c, 0.70f + n * 0.55f);
                    break;
                }
                case TILE_PLANKS: {
                    c = {0.66f, 0.50f, 0.30f};
                    const float line = (y % 5 == 0) ? 0.72f : 1.0f;
                    shade(c, line * (0.9f + n * 0.18f));
                    break;
                }
                case TILE_COBBLE: {
                    const int blob = static_cast<int>(rnd(x / 4, y / 4, 7) * 3.0f);
                    c = {0.45f, 0.45f, 0.47f};
                    shade(c, 0.70f + blob * 0.12f + n * 0.12f);
                    break;
                }
                case TILE_BEDROCK: {
                    c = {0.26f, 0.26f, 0.29f};
                    shade(c, 0.6f + n * 0.7f);
                    break;
                }
                default:
                    break;
            }
            putPixel(dst, x, y, c);
        }
    }
}

bool compile(const char* src, const char* entry, const char* target,
             ID3DBlob** blob, std::string& error) {
    ID3DBlob* errors = nullptr;
    const UINT flags = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(src, std::strlen(src), nullptr, nullptr, nullptr,
                                  entry, target, flags, 0, blob, &errors);
    if (FAILED(hr)) {
        if (errors) {
            error.assign(static_cast<const char*>(errors->GetBufferPointer()),
                         errors->GetBufferSize());
            errors->Release();
        } else {
            error = "shader compilation failed";
        }
        return false;
    }
    if (errors) errors->Release();
    return true;
}

} // namespace

void Frustum::fromViewProj(const Mat4& vp) {
    // Row-vector convention: clip = v * M, so planes come from matrix columns.
    const float* m = &vp.m[0][0];
    auto col = [&](int j, int i) { return m[i * 4 + j]; };

    for (int j = 0; j < 3; ++j) {
        planes[j * 2 + 0][0] = col(j, 0) + col(3, 0);
        planes[j * 2 + 0][1] = col(j, 1) + col(3, 1);
        planes[j * 2 + 0][2] = col(j, 2) + col(3, 2);
        planes[j * 2 + 0][3] = col(j, 3) + col(3, 3);

        planes[j * 2 + 1][0] = -col(j, 0) + col(3, 0);
        planes[j * 2 + 1][1] = -col(j, 1) + col(3, 1);
        planes[j * 2 + 1][2] = -col(j, 2) + col(3, 2);
        planes[j * 2 + 1][3] = -col(j, 3) + col(3, 3);
    }
}

bool Frustum::testAABB(const Vec3& mn, const Vec3& mx) const {
    for (const auto& p : planes) {
        const float px = (p[0] >= 0.0f) ? mx.x : mn.x;
        const float py = (p[1] >= 0.0f) ? mx.y : mn.y;
        const float pz = (p[2] >= 0.0f) ? mx.z : mn.z;
        if (p[0] * px + p[1] * py + p[2] * pz + p[3] < 0.0f) return false;
    }
    return true;
}

bool Renderer::init(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    if (!createSwapChain(hwnd, width, height)) return false;
    if (!createTargets()) return false;
    if (!createPipelines()) return false;
    if (!createTextureArray()) return false;

    D3D10_VIEWPORT vp{};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<UINT>(width_);
    vp.Height = static_cast<UINT>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    device_->RSSetViewports(1, &vp);

    return true;
}

bool Renderer::createSwapChain(HWND hwnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = static_cast<UINT>(width);
    sd.BufferDesc.Height = static_cast<UINT>(height);
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const HRESULT hr = D3D10CreateDeviceAndSwapChain(
        nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_SDK_VERSION,
        &sd, &swap_, &device_);
    return SUCCEEDED(hr);
}

bool Renderer::createTargets() {
    ID3D10Texture2D* backBuffer = nullptr;
    if (FAILED(swap_->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer))))
        return false;
    const HRESULT hr = device_->CreateRenderTargetView(backBuffer, nullptr, &rtv_);
    backBuffer->Release();
    if (FAILED(hr)) return false;

    D3D10_TEXTURE2D_DESC dd{};
    dd.Width = static_cast<UINT>(width_);
    dd.Height = static_cast<UINT>(height_);
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D10_USAGE_DEFAULT;
    dd.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    if (FAILED(device_->CreateTexture2D(&dd, nullptr, &depthTex_))) return false;
    if (FAILED(device_->CreateDepthStencilView(depthTex_, nullptr, &dsv_))) return false;

    device_->OMSetRenderTargets(1, &rtv_, dsv_);
    return true;
}

void Renderer::releaseTargets() {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
    if (dsv_) { dsv_->Release(); dsv_ = nullptr; }
    if (depthTex_) { depthTex_->Release(); depthTex_ = nullptr; }
}

bool Renderer::resize(int width, int height) {
    if (!swap_ || width <= 0 || height <= 0) return false;
    if (width == width_ && height == height_) return true;

    width_ = width;
    height_ = height;
    releaseTargets();

    if (FAILED(swap_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                    DXGI_FORMAT_UNKNOWN, 0)))
        return false;
    if (!createTargets()) return false;

    D3D10_VIEWPORT vp{};
    vp.Width = static_cast<UINT>(width_);
    vp.Height = static_cast<UINT>(height_);
    vp.MaxDepth = 1.0f;
    device_->RSSetViewports(1, &vp);
    return true;
}

bool Renderer::createPipelines() {
    std::string error;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    if (!compile(kTerrainShader, "VSMain", "vs_4_0", &vsBlob, error)) return false;
    if (!compile(kTerrainShader, "PSMain", "ps_4_0", &psBlob, error)) {
        vsBlob->Release();
        return false;
    }

    if (FAILED(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                           &vs_))) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }
    if (FAILED(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                          &ps_))) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    const D3D10_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 20, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0},
    };

    if (FAILED(device_->CreateInputLayout(layout, 4, vsBlob->GetBufferPointer(),
                                          vsBlob->GetBufferSize(), &inputLayout_))) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }
    vsBlob->Release();
    psBlob->Release();

    D3D10_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(Constants);
    cbd.Usage = D3D10_USAGE_DYNAMIC;
    cbd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&cbd, nullptr, &cb_))) return false;

    D3D10_SAMPLER_DESC sd{};
    sd.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D10_COMPARISON_NEVER;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D10_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&sd, &sampler_))) return false;

    D3D10_RASTERIZER_DESC rd{};
    rd.FillMode = D3D10_FILL_SOLID;
    rd.CullMode = D3D10_CULL_NONE; // mesher already drops hidden faces
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rd, &rasterSolid_))) return false;
    rd.FillMode = D3D10_FILL_WIREFRAME;
    if (FAILED(device_->CreateRasterizerState(&rd, &rasterWire_))) return false;

    D3D10_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D10_COMPARISON_LESS;
    if (FAILED(device_->CreateDepthStencilState(&dsd, &depthState_))) return false;

    D3D10_BLEND_DESC bd{};
    bd.AlphaToCoverageEnable = FALSE;
    bd.BlendEnable[0] = FALSE;
    bd.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&bd, &blendState_))) return false;

    // Entity pipeline: untextured, vertex-coloured boxes for mobs.
    {
        ID3D10Blob* evs = nullptr;
        ID3D10Blob* eps = nullptr;
        ID3D10Blob* errs = nullptr;
        if (FAILED(D3D10CompileShader(kEntityShader, strlen(kEntityShader), nullptr, nullptr,
                                      nullptr, "VSEntity", "vs_4_0", 0, &evs, &errs))) {
            if (errs) errs->Release();
            return false;
        }
        if (errs) { errs->Release(); errs = nullptr; }
        if (FAILED(D3D10CompileShader(kEntityShader, strlen(kEntityShader), nullptr, nullptr,
                                      nullptr, "PSEntity", "ps_4_0", 0, &eps, &errs))) {
            if (evs) evs->Release();
            if (errs) errs->Release();
            return false;
        }
        if (errs) errs->Release();
        device_->CreateVertexShader(evs->GetBufferPointer(), evs->GetBufferSize(), &entVS_);
        device_->CreatePixelShader(eps->GetBufferPointer(), eps->GetBufferSize(), &entPS_);
        D3D10_INPUT_ELEMENT_DESC eElems[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0},
        };
        device_->CreateInputLayout(eElems, 2, evs->GetBufferPointer(), evs->GetBufferSize(), &entLayout_);
        evs->Release();
        eps->Release();

        D3D10_BUFFER_DESC ebd{};
        ebd.ByteWidth = sizeof(float) * 7 * 36 * 512; // up to 512 boxes
        ebd.Usage = D3D10_USAGE_DYNAMIC;
        ebd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        ebd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
        device_->CreateBuffer(&ebd, nullptr, &entVB_);
    }

    return true;
}

bool Renderer::createTextureArray() {
    const UINT size = 16;
    const UINT tileCount = TILE_COUNT;
    const size_t n = static_cast<size_t>(tileCount) * size * size * 4;
    if (tilePixels_.empty()) {
        tilePixels_.assign(n, 0);
        for (UINT t = 0; t < tileCount; ++t)
            GenerateProceduralTile(static_cast<int>(t), tilePixels_.data() + static_cast<size_t>(t) * size * size * 4);
    }

    D3D10_TEXTURE2D_DESC td{};
    td.Width = size;
    td.Height = size;
    td.MipLevels = 1;
    td.ArraySize = tileCount;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D10_USAGE_IMMUTABLE;
    td.BindFlags = D3D10_BIND_SHADER_RESOURCE;

    std::vector<D3D10_SUBRESOURCE_DATA> init(tileCount);
    for (UINT t = 0; t < tileCount; ++t) {
        init[t].pSysMem = tilePixels_.data() + static_cast<size_t>(t) * size * size * 4;
        init[t].SysMemPitch = size * 4;
        init[t].SysMemSlicePitch = size * size * 4;
    }

    ID3D10Texture2D* tex = nullptr;
    if (FAILED(device_->CreateTexture2D(&td, init.data(), &tex))) return false;

    D3D10_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = td.Format;
    srv.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
    srv.Texture2DArray.MostDetailedMip = 0;
    srv.Texture2DArray.MipLevels = 1;
    srv.Texture2DArray.FirstArraySlice = 0;
    srv.Texture2DArray.ArraySize = tileCount;

    const HRESULT hr = device_->CreateShaderResourceView(tex, &srv, &texArray_);
    tex->Release();
    return SUCCEEDED(hr);
}

bool Renderer::loadTextures(const std::vector<LoadedTexture>& loaded) {
    loaded_ = loaded;
    texturesLoaded_ = true;

    const size_t n = static_cast<size_t>(TILE_COUNT) * 16 * 16 * 4;
    if (tilePixels_.size() != n) tilePixels_.assign(n, 0);

    // Debug aid: drop a file named DEBUG_TILES next to the exe to paint every
    // texture-array slice a solid diagnostic colour (tile 0 = red, etc.).
    FILE* dbgFile = fopen("DEBUG_TILES", "rb");
    const bool tileDebug = dbgFile != nullptr;
    if (dbgFile) fclose(dbgFile);
    if (tileDebug) {
        static const uint8_t dbg[TILE_COUNT][3] = {
            {255, 0, 0},     // 0 grass top
            {0, 255, 0},     // 1 grass side
            {0, 0, 255},     // 2 dirt
            {255, 255, 0},   // 3 stone
            {255, 0, 255},   // 4 sand
            {0, 255, 255},   // 5 water
            {128, 64, 0},    // 6 log side
            {192, 160, 64},  // 7 log top
            {0, 128, 0},     // 8 leaves
            {160, 96, 32},   // 9 planks
            {128, 128, 128}, // 10 cobble
            {64, 64, 64},    // 11 bedrock
        };
        for (int t = 0; t < TILE_COUNT; ++t) {
            uint8_t* p = tilePixels_.data() + static_cast<size_t>(t) * 16 * 16 * 4;
            for (int i = 0; i < 16 * 16; ++i) {
                p[i * 4 + 0] = dbg[t][0];
                p[i * 4 + 1] = dbg[t][1];
                p[i * 4 + 2] = dbg[t][2];
                p[i * 4 + 3] = 255;
            }
        }
        if (texArray_) { texArray_->Release(); texArray_ = nullptr; }
        return createTextureArray();
    }

    for (int t = 0; t < TILE_COUNT; ++t)
        GenerateProceduralTile(t, tilePixels_.data() + static_cast<size_t>(t) * 16 * 16 * 4);

    for (const LoadedTexture& lt : loaded) {
        if (lt.tile < 0 || lt.tile >= TILE_COUNT) continue;
        const PngInfo& png = lt.png;
        if (png.width <= 0 || png.height <= 0) continue;
        ScaleTo16(png.rgba.data(), png.width, png.height,
                 tilePixels_.data() + static_cast<size_t>(lt.tile) * 16 * 16 * 4);
    }

    // Biome tints: these textures are grayscale in the jar (vanilla tints them
    // at render time). Plains values: grass 0x91BD59, foliage 0x77AB2F,
    // water 0x3F76E4.
    auto tint = [&](int tile, int tr, int tg, int tb) {
        uint8_t* p = tilePixels_.data() + static_cast<size_t>(tile) * 16 * 16 * 4;
        for (int i = 0; i < 16 * 16; ++i) {
            p[i * 4 + 0] = static_cast<uint8_t>(p[i * 4 + 0] * tr / 255);
            p[i * 4 + 1] = static_cast<uint8_t>(p[i * 4 + 1] * tg / 255);
            p[i * 4 + 2] = static_cast<uint8_t>(p[i * 4 + 2] * tb / 255);
        }
    };
    tint(TILE_GRASS_TOP, 0x91, 0xBD, 0x59);
    tint(TILE_LEAVES, 0x77, 0xAB, 0x2F);
    tint(TILE_WATER, 0x3F, 0x76, 0xE4);

    // Upload the real jar pixels into the GPU texture array.
    if (texArray_) { texArray_->Release(); texArray_ = nullptr; }
    return createTextureArray();
}

void Renderer::uploadChunk(GpuChunk& gpu, const ChunkMesh& mesh, const Vec3& mn, const Vec3& mx) {
    gpu.min = mn;
    gpu.max = mx;
    gpu.indexCount = static_cast<UINT>(mesh.indices.size());

    if (gpu.vb) { gpu.vb->Release(); gpu.vb = nullptr; }
    if (gpu.ib) { gpu.ib->Release(); gpu.ib = nullptr; }

    if (mesh.vertices.empty() || mesh.indices.empty()) return;

    D3D10_BUFFER_DESC vbd{};
    vbd.ByteWidth = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));
    vbd.Usage = D3D10_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    D3D10_SUBRESOURCE_DATA vinit{mesh.vertices.data(), 0, 0};
    device_->CreateBuffer(&vbd, &vinit, &gpu.vb);

    D3D10_BUFFER_DESC ibd{};
    ibd.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
    ibd.Usage = D3D10_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D10_BIND_INDEX_BUFFER;
    D3D10_SUBRESOURCE_DATA iinit{mesh.indices.data(), 0, 0};
    device_->CreateBuffer(&ibd, &iinit, &gpu.ib);
}

RenderStats Renderer::render(const Mat4& viewProj, const Vec3& camPos,
                             const std::vector<GpuChunk>& chunks, bool wireframe) {
    RenderStats stats;
    stats.total = static_cast<int>(chunks.size());

    Frustum frustum;
    frustum.fromViewProj(viewProj);

    device_->ClearRenderTargetView(rtv_, kSkyColor);
    device_->ClearDepthStencilView(dsv_, D3D10_CLEAR_DEPTH, 1.0f, 0);

    Constants c{};
    std::memcpy(c.viewProj, viewProj.m, sizeof(c.viewProj));
    c.camPos[0] = camPos.x;
    c.camPos[1] = camPos.y;
    c.camPos[2] = camPos.z;
    c.fogColor[0] = kSkyColor[0];
    c.fogColor[1] = kSkyColor[1];
    c.fogColor[2] = kSkyColor[2];
    c.fogColor[3] = 1.0f;
    c.fogParams[0] = fogStart_;
    c.fogParams[1] = fogEnd_;

    void* mapped = nullptr;
    if (SUCCEEDED(cb_->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped))) {
        std::memcpy(mapped, &c, sizeof(c));
        cb_->Unmap();
    }

    device_->IASetInputLayout(inputLayout_);
    device_->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->VSSetShader(vs_);
    device_->PSSetShader(ps_);
    device_->VSSetConstantBuffers(0, 1, &cb_);
    device_->PSSetConstantBuffers(0, 1, &cb_);
    device_->PSSetShaderResources(0, 1, &texArray_);
    device_->PSSetSamplers(0, 1, &sampler_);
    device_->OMSetDepthStencilState(depthState_, 0);
    device_->OMSetBlendState(blendState_, nullptr, 0xFFFFFFFF);
    device_->RSSetState(wireframe ? rasterWire_ : rasterSolid_);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;

    for (const auto& ch : chunks) {
        if (ch.indexCount == 0 || !ch.vb || !ch.ib) continue;
        if (!frustum.testAABB(ch.min, ch.max)) continue;

        device_->IASetVertexBuffers(0, 1, &ch.vb, &stride, &offset);
        device_->IASetIndexBuffer(ch.ib, DXGI_FORMAT_R32_UINT, 0);
        device_->DrawIndexed(ch.indexCount, 0, 0);

        ++stats.drawn;
        stats.triangles += ch.indexCount / 3;
    }

    return stats;
}

void Renderer::present(bool vsync) {
    if (swap_) swap_->Present(vsync ? 1 : 0, 0);
}

void Renderer::clear(const float color[4]) {
    if (!device_) return;
    device_->ClearRenderTargetView(rtv_, color);
    device_->ClearDepthStencilView(dsv_, D3D10_CLEAR_DEPTH, 1.0f, 0);
}

void Renderer::renderEntities(const std::vector<EntityBox>& boxes) {
    if (boxes.empty() || !entVB_ || !entVS_ || !entPS_) {
        entityDrawCount = 0;
        return;
    }
    const size_t maxBoxes = 512;
    const size_t count = std::min(boxes.size(), maxBoxes);
    entityDrawCount = static_cast<int>(count);

    struct EV { float x, y, z, r, g, b, a; };
    std::vector<EV> verts;
    verts.reserve(count * 36);

    auto addQuad = [&](const EV& a, const EV& b, const EV& c, const EV& d) {
        verts.push_back(a); verts.push_back(b); verts.push_back(c);
        verts.push_back(a); verts.push_back(c); verts.push_back(d);
    };
    auto colorOf = [](uint32_t argb, float mul) {
        return EV{0, 0, 0,
                  static_cast<float>((argb >> 16) & 0xFF) / 255.0f * mul,
                  static_cast<float>((argb >> 8) & 0xFF) / 255.0f * mul,
                  static_cast<float>(argb & 0xFF) / 255.0f * mul, 1.0f};
    };

    for (const EntityBox& box : boxes) {
        const float hw = box.size.x * 0.5f;
        const float hd = box.size.z * 0.5f;
        const float y0 = box.pos.y;
        const float y1 = y0 + box.size.y;
        const float x0 = box.pos.x - hw, x1 = box.pos.x + hw;
        const float z0 = box.pos.z - hd, z1 = box.pos.z + hd;
        const EV p000 = colorOf(box.color, 0.72f), p100 = colorOf(box.color, 0.86f);
        const EV top = colorOf(box.color, 1.0f), bottom = colorOf(box.color, 0.5f);

        auto V = [](float x, float y, float z, const EV& c) {
            return EV{x, y, z, c.r, c.g, c.b, c.a};
        };
        // +Y / -Y
        addQuad(V(x0,y1,z0,top), V(x0,y1,z1,top), V(x1,y1,z1,top), V(x1,y1,z0,top));
        addQuad(V(x0,y0,z0,bottom), V(x1,y0,z0,bottom), V(x1,y0,z1,bottom), V(x0,y0,z1,bottom));
        // +/-X
        addQuad(V(x1,y0,z0,p100), V(x1,y1,z0,p100), V(x1,y1,z1,p100), V(x1,y0,z1,p100));
        addQuad(V(x0,y0,z0,p100), V(x0,y0,z1,p100), V(x0,y1,z1,p100), V(x0,y1,z0,p100));
        // +/-Z
        addQuad(V(x0,y0,z1,p000), V(x1,y0,z1,p000), V(x1,y1,z1,p000), V(x0,y1,z1,p000));
        addQuad(V(x0,y0,z0,p000), V(x0,y1,z0,p000), V(x1,y1,z0,p000), V(x1,y0,z0,p000));
    }

    void* mapped = nullptr;
    if (SUCCEEDED(entVB_->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped))) {
        std::memcpy(mapped, verts.data(), sizeof(EV) * verts.size());
        entVB_->Unmap();
    }

    device_->IASetInputLayout(entLayout_);
    device_->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->VSSetShader(entVS_);
    device_->PSSetShader(entPS_);
    device_->VSSetConstantBuffers(0, 1, &cb_);
    device_->PSSetConstantBuffers(0, 1, &cb_);
    device_->OMSetDepthStencilState(depthState_, 0);
    device_->OMSetBlendState(blendState_, nullptr, 0xFFFFFFFF);
    device_->RSSetState(rasterSolid_);
    const UINT stride = sizeof(EV);
    const UINT offset = 0;
    device_->IASetVertexBuffers(0, 1, &entVB_, &stride, &offset);
    device_->Draw(static_cast<UINT>(verts.size()), 0);
}

void Renderer::shutdown() {
    releaseTargets();
    if (swap_) { swap_->Release(); swap_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
    if (inputLayout_) { inputLayout_->Release(); inputLayout_ = nullptr; }
    if (vs_) { vs_->Release(); vs_ = nullptr; }
    if (ps_) { ps_->Release(); ps_ = nullptr; }
    if (cb_) { cb_->Release(); cb_ = nullptr; }
    if (sampler_) { sampler_->Release(); sampler_ = nullptr; }
    if (texArray_) { texArray_->Release(); texArray_ = nullptr; }
    if (rasterSolid_) { rasterSolid_->Release(); rasterSolid_ = nullptr; }
    if (rasterWire_) { rasterWire_->Release(); rasterWire_ = nullptr; }
    if (depthState_) { depthState_->Release(); depthState_ = nullptr; }
    if (blendState_) { blendState_->Release(); blendState_ = nullptr; }
    if (entLayout_) { entLayout_->Release(); entLayout_ = nullptr; }
    if (entVS_) { entVS_->Release(); entVS_ = nullptr; }
    if (entPS_) { entPS_->Release(); entPS_ = nullptr; }
    if (entVB_) { entVB_->Release(); entVB_ = nullptr; }
}
