// CraftEngine - Minecraft bitmap font (jar textures/font/ascii.png).
#pragma once
#include <cstdint>
#include <string>

#include "assets.h"
#include "gui.h"

class BitmapFont {
public:
    bool load(const Assets& assets, Gui& gui);
    float measure(const std::string& text, float scale) const;
    void draw(Gui& gui, float x, float y, const std::string& text,
              uint32_t argb, bool shadow, float scale) const;
    // Rotates the whole run around (pivotX, pivotY).
    void drawRot(Gui& gui, float pivotX, float pivotY, const std::string& text,
                 uint32_t argb, bool shadow, float scale, float angleRad) const;
    bool loaded() const { return tex_ >= 0; }
    int texId() const { return tex_; }

private:
    void drawRun(Gui& gui, float x, float y, const std::string& text,
                 uint32_t argb, float scale) const;

    Gui* gui_ = nullptr;
    int tex_ = -1;
    int atlasW_ = 0;
    int atlasH_ = 0;
    int cellW_ = 8;
    int cellH_ = 8;
    int widths_[256]{};
};
