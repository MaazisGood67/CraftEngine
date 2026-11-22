#include "font.h"

#include <cmath>
#include <cstring>

#include "png.h"

bool BitmapFont::load(const Assets& assets, Gui& gui) {
    gui_ = &gui;
    const std::vector<uint8_t> data = assets.read("minecraft/textures/font/ascii.png");
    if (data.empty()) return false;
    PngInfo png;
    if (!DecodePng(data.data(), data.size(), png)) return false;
    if (png.width <= 0 || png.height <= 0) return false;

    atlasW_ = png.width;
    atlasH_ = png.height;
    cellW_ = atlasW_ / 16;
    cellH_ = atlasH_ / 16;
    if (cellW_ <= 0 || cellH_ <= 0) return false;

    // Advance = rightmost opaque column + 1 (vanilla glyph bake does the same).
    for (int cp = 0; cp < 256; ++cp) {
        const int cx = (cp % 16) * cellW_;
        const int cy = (cp / 16) * cellH_;
        int right = -1;
        for (int y = 0; y < cellH_; ++y) {
            for (int x = 0; x < cellW_; ++x) {
                const size_t o = (static_cast<size_t>(cy + y) * atlasW_ + cx + x) * 4;
                if (png.rgba[o + 3] > 0 && x > right) right = x;
            }
        }
        widths_[cp] = right + 1;
    }
    widths_[static_cast<int>(' ')] = 4; // space has no pixels

    tex_ = gui.createTexture(png.rgba.data(), png.width, png.height);
    return tex_ >= 0;
}

float BitmapFont::measure(const std::string& text, float scale) const {
    float w = 0;
    for (unsigned char c : text) {
        if (c == '\n') continue;
        w += static_cast<float>(widths_[c] + 1) * scale;
    }
    if (!text.empty()) w -= scale; // no trailing gap
    return w;
}

void BitmapFont::drawRun(Gui& gui, float x, float y, const std::string& text,
                         uint32_t argb, float scale) const {
    const float invW = 1.0f / static_cast<float>(atlasW_);
    const float invH = 1.0f / static_cast<float>(atlasH_);
    float pen = x;
    for (unsigned char c : text) {
        if (c == '\n') continue;
        const int gw = widths_[c];
        const int cx = (c % 16) * cellW_;
        const int cy = (c / 16) * cellH_;
        if (gw > 0) {
            gui.quad(tex_, pen, y, static_cast<float>(gw) * scale, static_cast<float>(cellH_) * scale,
                     static_cast<float>(cx) * invW, static_cast<float>(cy) * invH,
                     static_cast<float>(cx + gw) * invW, static_cast<float>(cy + cellH_) * invH,
                     argb);
        }
        pen += static_cast<float>(gw + 1) * scale;
    }
}

void BitmapFont::draw(Gui& gui, float x, float y, const std::string& text,
                      uint32_t argb, bool shadow, float scale) const {
    if (tex_ < 0) return;
    if (shadow) {
        const uint32_t dark = 0xFF000000u | ((argb >> 2) & 0x3F3F3Fu);
        drawRun(gui, x + scale, y + scale, text, dark, scale);
    }
    drawRun(gui, x, y, text, argb, scale);
}

void BitmapFont::drawRot(Gui& gui, float pivotX, float pivotY, const std::string& text,
                         uint32_t argb, bool shadow, float scale, float angleRad) const {
    if (tex_ < 0) return;
    const float w = measure(text, scale);
    const float invW = 1.0f / static_cast<float>(atlasW_);
    const float invH = 1.0f / static_cast<float>(atlasH_);
    const float cs = std::cos(angleRad);
    const float sn = std::sin(angleRad);

    auto run = [&](float offX, float offY, uint32_t color) {
        float pen = -w * 0.5f + offX;
        const float top = -4.0f * scale + offY;
        for (unsigned char c : text) {
            if (c == '\n') continue;
            const int gw = widths_[c];
            if (gw > 0) {
                const float lx = pen + static_cast<float>(gw) * scale * 0.5f;
                const float ly = top + static_cast<float>(cellH_) * scale * 0.5f;
                const float rx = pivotX + lx * cs - ly * sn;
                const float ry = pivotY + lx * sn + ly * cs;
                const int cx = (c % 16) * cellW_;
                const int cy = (c / 16) * cellH_;
                gui.quadRot(tex_, rx, ry, static_cast<float>(gw) * scale,
                            static_cast<float>(cellH_) * scale, angleRad,
                            static_cast<float>(cx) * invW, static_cast<float>(cy) * invH,
                            static_cast<float>(cx + gw) * invW,
                            static_cast<float>(cy + cellH_) * invH, color);
            }
            pen += static_cast<float>(gw + 1) * scale;
        }
    };

    if (shadow) {
        const uint32_t dark = 0xFF000000u | ((argb >> 2) & 0x3F3F3Fu);
        run(scale, scale, dark);
    }
    run(0.0f, 0.0f, argb);
}
