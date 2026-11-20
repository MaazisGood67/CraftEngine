// CraftEngine - PNG decoder -> RGBA8.
// Supports 8-bit truecolor (RGB/RGBA), gray (+alpha), and palette/indexed
// (colorType 3) at 1/2/4/8-bit with PLTE + tRNS. Non-interlaced only.
#include "png.h"

#include <zlib.h>

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {

uint32_t rd32p(const uint8_t* p) {
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

// Extract one sub-byte sample (bitDepth 1/2/4/8) for palette/gray pixels.
int sampleAt(const uint8_t* row, size_t x, int bd) {
    if (bd == 8) return row[x];
    const size_t bit = x * static_cast<size_t>(bd);
    const int shift = 8 - bd - static_cast<int>(bit % 8);
    return (row[bit / 8] >> shift) & ((1 << bd) - 1);
}

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
    std::vector<uint8_t> idat, plte, trns;

    size_t pos = 8;
    while (pos + 8 <= size) {
        const uint32_t len = rd32p(&data[pos]);
        const uint32_t type = rd32p(&data[pos + 4]);
        const size_t bodyOff = pos + 8;
        if (bodyOff + len > size) break;
        if (type == 0x49484452u /* IHDR */) {
            width = rd32p(&data[bodyOff]);
            height = rd32p(&data[bodyOff + 4]);
            bitDepth = data[bodyOff + 8];
            colorType = data[bodyOff + 9];
            interlace = data[bodyOff + 12];
        } else if (type == 0x504C5445u /* PLTE */) {
            plte.assign(&data[bodyOff], &data[bodyOff + len]);
        } else if (type == 0x74524E53u /* tRNS */) {
            trns.assign(&data[bodyOff], &data[bodyOff + len]);
        } else if (type == 0x49444154u /* IDAT */) {
            idat.insert(idat.end(), &data[bodyOff], &data[bodyOff + len]);
        } else if (type == 0x49454E44u /* IEND */) {
            break;
        }
        pos = bodyOff + len + 4; // + CRC
    }

    if (width == 0 || height == 0 || interlace != 0) return false;

    int channels = 0;
    switch (colorType) {
        case 0: channels = 1; if (bitDepth != 1 && bitDepth != 2 && bitDepth != 4 && bitDepth != 8) return false; break;
        case 2: channels = 3; if (bitDepth != 8) return false; break;
        case 3: channels = 1; if (bitDepth != 1 && bitDepth != 2 && bitDepth != 4 && bitDepth != 8) return false; if (plte.empty()) return false; break;
        case 4: channels = 2; if (bitDepth != 8) return false; break;
        case 6: channels = 4; if (bitDepth != 8) return false; break;
        default: return false;
    }

    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    const int bppBits = channels * bitDepth;
    const size_t stride = (w * static_cast<size_t>(bppBits) + 7) / 8;
    int filterBpp = (bppBits + 7) / 8;
    if (filterBpp < 1) filterBpp = 1;

    z_stream zs{};
    if (inflateInit2(&zs, 15) != Z_OK) return false;
    std::vector<uint8_t> raw(stride * h + h, 0); // +1 filter byte per row
    zs.next_in = const_cast<Bytef*>(idat.data());
    zs.avail_in = static_cast<uInt>(idat.size());
    zs.next_out = raw.data();
    zs.avail_out = static_cast<uInt>(raw.size());
    const int ret = inflate(&zs, Z_FINISH);
    const bool ok = (ret == Z_STREAM_END);
    inflateEnd(&zs);
    if (!ok) return false;

    // Unfilter scanlines (filter bpp = bytes per pixel, min 1).
    std::vector<uint8_t> recon(stride * h, 0);
    for (size_t y = 0; y < h; ++y) {
        const size_t rowOff = y * (stride + 1);
        const uint8_t filter = raw[rowOff];
        const uint8_t* rowIn = &raw[rowOff + 1];
        uint8_t* rowOut = &recon[y * stride];
        for (size_t i = 0; i < stride; ++i) {
            const int s = rowIn[i];
            const int a = (i >= static_cast<size_t>(filterBpp)) ? rowOut[i - filterBpp] : 0;
            const int b = (y > 0) ? recon[(y - 1) * stride + i] : 0;
            const int c = (y > 0 && i >= static_cast<size_t>(filterBpp)) ? recon[(y - 1) * stride + i - filterBpp] : 0;
            int v = s;
            switch (filter) {
                case 0: v = s; break;
                case 1: v = s + a; break;
                case 2: v = s + b; break;
                case 3: v = s + (a + b) / 2; break;
                case 4: v = s + paeth(a, b, c); break;
            }
            rowOut[i] = static_cast<uint8_t>(v);
        }
    }

    // Expand to RGBA8.
    const int maxval = (1 << bitDepth) - 1;
    std::vector<uint8_t> img(w * h * 4, 0);
    for (size_t y = 0; y < h; ++y) {
        const uint8_t* row = &recon[y * stride];
        for (size_t x = 0; x < w; ++x) {
            uint8_t* d = &img[(y * w + x) * 4];
            if (colorType == 3) {
                const int idx = sampleAt(row, x, bitDepth);
                if (static_cast<size_t>(idx) * 3 + 2 >= plte.size()) {
                    d[0] = d[1] = d[2] = 0; d[3] = 255;
                } else {
                    d[0] = plte[idx * 3]; d[1] = plte[idx * 3 + 1]; d[2] = plte[idx * 3 + 2];
                    d[3] = (static_cast<size_t>(idx) < trns.size()) ? trns[idx] : 255;
                }
            } else if (colorType == 0) {
                const int s = sampleAt(row, x, bitDepth);
                const int g = (bitDepth == 8) ? s : (s * 255) / maxval;
                int alpha = 255;
                if (trns.size() >= 2) {
                    const int key = (trns[0] << 8) | trns[1];
                    if (s == key) alpha = 0;
                }
                d[0] = d[1] = d[2] = static_cast<uint8_t>(g); d[3] = static_cast<uint8_t>(alpha);
            } else {
                const uint8_t* s = &row[x * static_cast<size_t>(channels)];
                if (colorType == 6) {
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
                } else if (colorType == 2) {
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
                    if (trns.size() >= 6 && s[0] == trns[1] && s[1] == trns[3] && s[2] == trns[5]) d[3] = 0;
                } else { // colorType 4
                    d[0] = d[1] = d[2] = s[0]; d[3] = s[1];
                }
            }
        }
    }

    info.width = static_cast<int>(width);
    info.height = static_cast<int>(height);
    info.rgba = std::move(img);
    return true;
}