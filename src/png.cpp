// CraftEngine - PNG decoder (all PNG color types, bit depths 1/2/4/8/16,
// non-interlaced) -> RGBA8. Minecraft 26.3 ships most block textures as
// 4-bit palette PNGs, so palette + tRNS support is required.
#include "png.h"

#include <zlib.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

uint32_t rd32be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

inline uint32_t chunkType(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

constexpr uint32_t kIHDR = 0x49484452u;
constexpr uint32_t kPLTE = 0x504C5445u;
constexpr uint32_t ktRNS = 0x74524E53u;
constexpr uint32_t kIDAT = 0x49444154u;
constexpr uint32_t kIEND = 0x49454E44u;

} // namespace

bool DecodePng(const uint8_t* data, size_t size, PngInfo& info) {
    info.width = 0;
    info.height = 0;
    info.rgba.clear();

    if (size < 8 || data[0] != 0x89 || data[1] != 'P' || data[2] != 'N' || data[3] != 'G' ||
        data[4] != 0x0D || data[5] != 0x0A || data[6] != 0x1A || data[7] != 0x0A) {
        return false;
    }

    uint32_t width = 0, height = 0;
    int bitDepth = 0, colorType = -1, interlace = 0;
    std::vector<uint8_t> idat;
    std::vector<uint8_t> palette; // RGB triplets
    std::vector<uint8_t> trns;

    size_t pos = 8;
    bool sawIhdr = false;
    while (pos + 8 <= size) {
        const uint32_t len = rd32be(&data[pos]);
        const uint32_t type = chunkType(&data[pos + 4]);
        const size_t body = pos + 8;
        if (body + len + 4 > size) break; // truncated chunk (CRC included)

        if (type == kIHDR) {
            if (len < 13) return false;
            width = rd32be(&data[body + 0]);
            height = rd32be(&data[body + 4]);
            bitDepth = data[body + 8];
            colorType = data[body + 9];
            interlace = data[body + 12];
            sawIhdr = true;
        } else if (type == kPLTE) {
            palette.assign(&data[body], &data[body] + len);
        } else if (type == ktRNS) {
            trns.assign(&data[body], &data[body] + len);
        } else if (type == kIDAT) {
            idat.insert(idat.end(), &data[body], &data[body] + len);
        } else if (type == kIEND) {
            break;
        }
        pos = body + len + 4;
    }

    if (!sawIhdr || width == 0 || height == 0 || interlace != 0) return false;
    if (idat.empty()) return false;

    int channels = 0;
    switch (colorType) {
        case 0: channels = 1; break; // gray
        case 2: channels = 3; break; // rgb
        case 3: channels = 1; break; // palette index
        case 4: channels = 2; break; // gray + alpha
        case 6: channels = 4; break; // rgba
        default: return false;
    }
    const bool paletteOk = (colorType == 3 && !palette.empty() && (bitDepth == 1 || bitDepth == 2 ||
                                                                   bitDepth == 4 || bitDepth == 8));
    if (colorType == 3 && !paletteOk) return false;
    if (colorType != 3 && bitDepth != 8 && bitDepth != 16 &&
        !(colorType == 0 && (bitDepth == 1 || bitDepth == 2 || bitDepth == 4))) {
        return false;
    }

    const size_t w = width;
    const size_t h = height;
    const int bitsPerPixel = channels * bitDepth;
    const size_t rowBytes = (w * static_cast<size_t>(bitsPerPixel) + 7) / 8;
    const size_t bpp = (bitsPerPixel + 7) / 8; // filter byte distance

    std::vector<uint8_t> raw(rowBytes * h + h);
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) return false;
    zs.next_in = const_cast<Bytef*>(idat.data());
    zs.avail_in = static_cast<uInt>(idat.size());
    zs.next_out = raw.data();
    zs.next_out = raw.data();
    zs.avail_out = static_cast<uInt>(raw.size());
    const int zr = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (zr != Z_STREAM_END && zs.total_out < raw.size()) return false;

    // Unfilter into `recon` (flat, rowBytes per row).
    std::vector<uint8_t> recon(rowBytes * h, 0);
    for (size_t y = 0; y < h; ++y) {
        const size_t rowOff = y * (rowBytes + 1);
        const uint8_t filter = raw[rowOff];
        const uint8_t* in = &raw[rowOff + 1];
        uint8_t* out = &recon[y * rowBytes];
        for (size_t i = 0; i < rowBytes; ++i) {
            const int s = in[i];
            const int a = (i >= bpp) ? out[i - bpp] : 0;
            const int b = (y > 0) ? recon[(y - 1) * rowBytes + i] : 0;
            const int c = (y > 0 && i >= bpp) ? recon[(y - 1) * rowBytes + i - bpp] : 0;
            int v = s;
            switch (filter) {
                case 0: v = s; break;
                case 1: v = s + a; break;
                case 2: v = s + b; break;
                case 3: v = s + (a + b) / 2; break;
                case 4: v = s + paeth(a, b, c); break;
                default: return false;
            }
            out[i] = static_cast<uint8_t>(v & 0xFF);
        }
    }

    info.width = static_cast<int>(width);
    info.height = static_cast<int>(height);
    info.rgba.resize(w * h * 4);

    const int maxVal = (1 << bitDepth) - 1;
    auto sample = [&](size_t row, size_t x) -> int {
        if (bitDepth == 8) return recon[row * rowBytes + x];
        if (bitDepth == 16) return recon[row * rowBytes + x * 2]; // high byte
        const size_t bitPos = x * static_cast<size_t>(bitDepth);
        const int shift = 8 - bitDepth - static_cast<int>(bitPos % 8);
        return (recon[row * rowBytes + bitPos / 8] >> shift) & maxVal;
    };

    for (size_t y = 0; y < h; ++y) {
        for (size_t x = 0; x < w; ++x) {
            uint8_t r = 0, g = 0, b = 0, a = 255;
            if (colorType == 3) {
                const int idx = sample(y, x);
                const size_t pi = static_cast<size_t>(idx) * 3;
                if (pi + 2 < palette.size()) {
                    r = palette[pi + 0];
                    g = palette[pi + 1];
                    b = palette[pi + 2];
                }
                if (static_cast<size_t>(idx) < trns.size()) a = trns[static_cast<size_t>(idx)];
            } else if (colorType == 0) {
                int v = sample(y, x);
                if (bitDepth < 8) v = v * 255 / maxVal;
                r = g = b = static_cast<uint8_t>(v);
            } else if (colorType == 4) {
                const int v = sample(y, x * 2);
                const int av = (bitDepth == 16) ? recon[y * rowBytes + x * 4 + 2]
                                                : recon[y * rowBytes + x * 2 + 1];
                r = g = b = static_cast<uint8_t>(v);
                a = static_cast<uint8_t>(av);
            } else if (colorType == 2) {
                const int step = (bitDepth == 16) ? 2 : 1;
                r = recon[y * rowBytes + x * 3 * step + 0];
                g = recon[y * rowBytes + x * 3 * step + step];
                b = recon[y * rowBytes + x * 3 * step + step * 2];
            } else { // 6
                const int step = (bitDepth == 16) ? 2 : 1;
                const size_t o = y * rowBytes + x * 4 * step;
                r = recon[o + 0];
                g = recon[o + step];
                b = recon[o + step * 2];
                a = recon[o + step * 3];
            }
            const size_t o = (y * w + x) * 4;
            info.rgba[o + 0] = r;
            info.rgba[o + 1] = g;
            info.rgba[o + 2] = b;
            info.rgba[o + 3] = a;
        }
    }
    return true;
}
