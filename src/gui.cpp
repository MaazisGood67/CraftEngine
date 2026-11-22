#include "gui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "png.h"
#include "shaders.h"

namespace {

constexpr int kMaxQuads = 4096;

ID3D10Blob* CompileShader(const char* src, const char* entry, const char* profile) {
    ID3D10Blob* code = nullptr;
    ID3D10Blob* errors = nullptr;
    const HRESULT hr = D3D10CompileShader(src, static_cast<SIZE_T>(strlen(src)), nullptr,
                                          nullptr, nullptr,
                                          entry, profile, D3D10_SHADER_OPTIMIZATION_LEVEL3,
                                          &code, &errors);
    if (errors) errors->Release();
    if (FAILED(hr)) {
        if (code) { code->Release(); code = nullptr; }
        return nullptr;
    }
    return code;
}

} // namespace

bool Gui::init(ID3D10Device* device) {
    device_ = device;

    ID3D10Blob* vsCode = CompileShader(kGuiShader, "VSGui", "vs_4_0");
    ID3D10Blob* psCode = CompileShader(kGuiShader, "PSGui", "ps_4_0");
    if (!vsCode || !psCode) return false;

    device_->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &vs_);
    device_->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), &ps_);

    D3D10_INPUT_ELEMENT_DESC elems[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0},
    };
    device_->CreateInputLayout(elems, 3, vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &layout_);
    vsCode->Release();
    psCode->Release();

    D3D10_BUFFER_DESC cbd{};
    cbd.ByteWidth = 16;
    cbd.Usage = D3D10_USAGE_DYNAMIC;
    cbd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&cbd, nullptr, &cb_);

    D3D10_BUFFER_DESC vbd{};
    vbd.ByteWidth = static_cast<UINT>(sizeof(Vert) * kMaxQuads * 4);
    vbd.Usage = D3D10_USAGE_DYNAMIC;
    vbd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&vbd, nullptr, &vb_);

    D3D10_BUFFER_DESC ibd{};
    ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * kMaxQuads * 6);
    ibd.Usage = D3D10_USAGE_DYNAMIC;
    ibd.BindFlags = D3D10_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&ibd, nullptr, &ib_);

    D3D10_SAMPLER_DESC sd{};
    sd.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D10_COMPARISON_NEVER;
    sd.MaxLOD = D3D10_FLOAT32_MAX;
    device_->CreateSamplerState(&sd, &sampler_);

    D3D10_BLEND_DESC bd{};
    bd.BlendEnable[0] = TRUE;
    bd.SrcBlend = D3D10_BLEND_SRC_ALPHA;
    bd.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
    bd.BlendOp = D3D10_BLEND_OP_ADD;
    bd.SrcBlendAlpha = D3D10_BLEND_ONE;
    bd.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    bd.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    bd.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
    device_->CreateBlendState(&bd, &blend_);

    D3D10_RASTERIZER_DESC rd{};
    rd.FillMode = D3D10_FILL_SOLID;
    rd.CullMode = D3D10_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rd, &raster_);

    // 2D drawing must not depth-test against itself: every quad sits at z=0.
    D3D10_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;
    ds.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc = D3D10_COMPARISON_ALWAYS;
    ds.StencilEnable = FALSE;
    device_->CreateDepthStencilState(&ds, &depthOff_);

    // Texture 0 = 1x1 white.
    const uint8_t white[4] = {255, 255, 255, 255};
    createTexture(white, 1, 1);
    verts_.reserve(static_cast<size_t>(kMaxQuads) * 4);
    return true;
}

void Gui::shutdown() {
    for (auto*& t : textures_) {
        if (t) { t->Release(); t = nullptr; }
    }
    if (vs_) { vs_->Release(); vs_ = nullptr; }
    if (ps_) { ps_->Release(); ps_ = nullptr; }
    if (layout_) { layout_->Release(); layout_ = nullptr; }
    if (cb_) { cb_->Release(); cb_ = nullptr; }
    if (vb_) { vb_->Release(); vb_ = nullptr; }
    if (ib_) { ib_->Release(); ib_ = nullptr; }
    if (sampler_) { sampler_->Release(); sampler_ = nullptr; }
    if (blend_) { blend_->Release(); blend_ = nullptr; }
    if (raster_) { raster_->Release(); raster_ = nullptr; }
    if (depthOff_) { depthOff_->Release(); depthOff_ = nullptr; }
    device_ = nullptr;
}

int Gui::createTexture(const uint8_t* rgba, int w, int h) {
    if (!device_ || w <= 0 || h <= 0) return -1;
    D3D10_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(w);
    desc.Height = static_cast<UINT>(h);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D10_USAGE_IMMUTABLE;
    desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    D3D10_SUBRESOURCE_DATA init{rgba, static_cast<UINT>(w * 4), 0};
    ID3D10Texture2D* tex = nullptr;
    if (FAILED(device_->CreateTexture2D(&desc, &init, &tex))) return -1;
    ID3D10ShaderResourceView* view = nullptr;
    device_->CreateShaderResourceView(tex, nullptr, &view);
    tex->Release();
    if (!view) return -1;
    textures_.push_back(view);
    return static_cast<int>(textures_.size()) - 1;
}

ID3D10ShaderResourceView* Gui::srv(int tex) const {
    if (tex < 0 || tex >= static_cast<int>(textures_.size())) return nullptr;
    return textures_[tex];
}

void Gui::begin(int screenW, int screenH) {
    screenW_ = screenW;
    screenH_ = screenH;
    verts_.clear();
    currentTex_ = -1;

    void* mapped = nullptr;
    if (SUCCEEDED(cb_->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped))) {
        const float data[4] = {static_cast<float>(screenW), static_cast<float>(screenH), 0.0f, 0.0f};
        std::memcpy(mapped, data, sizeof(data));
        cb_->Unmap();
    }

    device_->IASetInputLayout(layout_);
    device_->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_->VSSetShader(vs_);
    device_->PSSetShader(ps_);
    device_->VSSetConstantBuffers(0, 1, &cb_);
    device_->PSSetConstantBuffers(0, 1, &cb_);
    device_->PSSetSamplers(0, 1, &sampler_);
    device_->OMSetDepthStencilState(depthOff_, 0);
    device_->OMSetBlendState(blend_, nullptr, 0xFFFFFFFF);
    device_->RSSetState(raster_);
}

void Gui::flush() {
    if (verts_.empty() || currentTex_ < 0) {
        verts_.clear();
        return;
    }
    ID3D10ShaderResourceView* view = srv(currentTex_);
    if (!view) {
        verts_.clear();
        return;
    }

    const int quads = static_cast<int>(verts_.size() / 4);
    void* mapped = nullptr;
    if (SUCCEEDED(vb_->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped))) {
        std::memcpy(mapped, verts_.data(), sizeof(Vert) * verts_.size());
        vb_->Unmap();
    }
    if (SUCCEEDED(ib_->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped))) {
        uint32_t* idx = static_cast<uint32_t*>(mapped);
        for (int q = 0; q < quads; ++q) {
            const uint32_t b = static_cast<uint32_t>(q * 4);
            idx[q * 6 + 0] = b + 0;
            idx[q * 6 + 1] = b + 1;
            idx[q * 6 + 2] = b + 2;
            idx[q * 6 + 3] = b + 0;
            idx[q * 6 + 4] = b + 2;
            idx[q * 6 + 5] = b + 3;
        }
        ib_->Unmap();
    }

    const UINT stride = sizeof(Vert);
    const UINT offset = 0;
    device_->IASetVertexBuffers(0, 1, &vb_, &stride, &offset);
    device_->IASetIndexBuffer(ib_, DXGI_FORMAT_R32_UINT, 0);
    device_->PSSetShaderResources(0, 1, &view);
    device_->DrawIndexed(static_cast<UINT>(quads * 6), 0, 0);

    verts_.clear();
    currentTex_ = -1;
}

void Gui::end() {
    flush();
}

void Gui::quad(int tex, float x, float y, float w, float h,
               float u0, float v0, float u1, float v1, uint32_t argb) {
    if (tex < 0) return;
    if (currentTex_ != tex) {
        flush();
        currentTex_ = tex;
    }
    if (static_cast<int>(verts_.size() / 4) >= kMaxQuads) flush();
    if (currentTex_ < 0) currentTex_ = tex;

    const float a = static_cast<float>((argb >> 24) & 0xFF) / 255.0f;
    const float r = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(argb & 0xFF) / 255.0f;

    verts_.push_back({x, y, u0, v0, r, g, b, a});
    verts_.push_back({x + w, y, u1, v0, r, g, b, a});
    verts_.push_back({x + w, y + h, u1, v1, r, g, b, a});
    verts_.push_back({x, y + h, u0, v1, r, g, b, a});
}

void Gui::quadRot(int tex, float cx, float cy, float w, float h, float angleRad,
                  float u0, float v0, float u1, float v1, uint32_t argb) {
    if (tex < 0) return;
    if (currentTex_ != tex) {
        flush();
        currentTex_ = tex;
    }
    if (static_cast<int>(verts_.size() / 4) >= kMaxQuads) flush();
    if (currentTex_ < 0) currentTex_ = tex;

    const float a = static_cast<float>((argb >> 24) & 0xFF) / 255.0f;
    const float r = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(argb & 0xFF) / 255.0f;

    const float cs = std::cos(angleRad);
    const float sn = std::sin(angleRad);
    const float hw = w * 0.5f;
    const float hh = h * 0.5f;
    const float lx[4] = {-hw, hw, hw, -hw};
    const float ly[4] = {-hh, -hh, hh, hh};
    for (int i = 0; i < 4; ++i) {
        const float rx = lx[i] * cs - ly[i] * sn;
        const float ry = lx[i] * sn + ly[i] * cs;
        verts_.push_back({cx + rx, cy + ry, i == 1 || i == 2 ? u1 : u0, i >= 2 ? v1 : v0, r, g, b, a});
    }
}

void Gui::rect(float x, float y, float w, float h, uint32_t argb) {
    quad(0, x, y, w, h, 0, 0, 1, 1, argb);
}

int LoadPngTexture(Gui& gui, const Assets& assets, const char* name, int* outW, int* outH) {
    const std::vector<uint8_t> data = assets.read(name);
    if (data.empty()) return -1;
    PngInfo png;
    if (!DecodePng(data.data(), data.size(), png)) return -1;
    if (outW) *outW = png.width;
    if (outH) *outH = png.height;
    return gui.createTexture(png.rgba.data(), png.width, png.height);
}

bool Panorama::load(ID3D10Device* device, Gui& gui, const Assets& assets) {
    ID3D10Blob* vsCode = CompileShader(kPanoramaShader, "VSPano", "vs_4_0");
    ID3D10Blob* psCode = CompileShader(kPanoramaShader, "PSPano", "ps_4_0");
    if (!vsCode || !psCode) return false;
    device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &vs_);
    device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), &ps_);
    vsCode->Release();
    psCode->Release();

    D3D10_BUFFER_DESC cbd{};
    cbd.ByteWidth = 32;
    cbd.Usage = D3D10_USAGE_DYNAMIC;
    cbd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cbd, nullptr, &cb_);

    D3D10_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;
    ds.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc = D3D10_COMPARISON_ALWAYS;
    ds.StencilEnable = FALSE;
    device->CreateDepthStencilState(&ds, &depthOff_);

    D3D10_SAMPLER_DESC sd{};
    sd.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D10_COMPARISON_NEVER;
    sd.MaxLOD = D3D10_FLOAT32_MAX;
    device->CreateSamplerState(&sd, &sampler_);

    int found = 0;
    for (int i = 0; i < 6; ++i) {
        char name[96];
        snprintf(name, sizeof(name), "minecraft/textures/gui/title/background/panorama_%d.png", i);
        faces_[i] = LoadPngTexture(gui, assets, name);
        if (faces_[i] >= 0) ++found;
    }
    ready_ = found == 6;
    return ready_;
}

void Panorama::draw(ID3D10Device* device, Gui& gui, int screenW, int screenH,
                    float yawRad, float pitchRad, float tanHalfFov, float aspect) {
    if (!ready_) return;

    void* mapped = nullptr;
    if (SUCCEEDED(cb_->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped))) {
        float data[8] = {static_cast<float>(screenW), static_cast<float>(screenH),
                         tanHalfFov, aspect, yawRad, pitchRad, 0.0f, 0.0f};
        std::memcpy(mapped, data, sizeof(data));
        cb_->Unmap();
    }

    D3D10_VIEWPORT vp{0, 0, static_cast<UINT>(screenW), static_cast<UINT>(screenH), 0.0f, 1.0f};
    device->RSSetViewports(1, &vp);
    device->IASetInputLayout(nullptr);
    device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device->VSSetShader(vs_);
    device->PSSetShader(ps_);
    device->VSSetConstantBuffers(0, 1, &cb_);
    device->PSSetConstantBuffers(0, 1, &cb_);
    device->OMSetDepthStencilState(depthOff_, 0);

    ID3D10ShaderResourceView* views[6];
    for (int i = 0; i < 6; ++i) views[i] = gui.srv(faces_[i]);
    device->PSSetShaderResources(0, 6, views);
    device->PSSetSamplers(0, 1, &sampler_);

    device->Draw(3, 0);
}

void Panorama::shutdown(ID3D10Device* device) {
    if (vs_) { vs_->Release(); vs_ = nullptr; }
    if (ps_) { ps_->Release(); ps_ = nullptr; }
    if (cb_) { cb_->Release(); cb_ = nullptr; }
    if (depthOff_) { depthOff_->Release(); depthOff_ = nullptr; }
    if (sampler_) { sampler_->Release(); sampler_ = nullptr; }
    ready_ = false;
}
