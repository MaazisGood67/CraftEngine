// Test: decode EVERY .png in 26.3.jar with DecodePng; tally OK/FAIL and report
// failures with their real (big-endian) IHDR colorType/bitDepth.
#include "png.h"
#include "zip.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

static uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

int main() {
    const std::string jarPath = "C:\\Users\\Administrator\\AppData\\Roaming\\Migurinth\\meta\\versions\\26.3\\26.3.jar";
    std::vector<ZipEntry> entries;
    std::string err;
    if (!LoadZip(jarPath, entries, err)) {
        printf("zip fail: %s\n", err.c_str());
        return 1;
    }
    int total = 0, ok = 0, fail = 0, shown = 0;
    int paletteOk = 0;
    for (const auto& e : entries) {
        if (e.name.size() < 4 || e.name.compare(e.name.size() - 4, 4, ".png") != 0) continue;
        if (e.data.size() < 33) continue;
        ++total;
        const uint8_t* d = e.data.data();
        int ct = d[25], bd = d[24];
        PngInfo info;
        if (DecodePng(d, e.data.size(), info)) {
            ++ok;
            if (ct == 3) ++paletteOk;
        } else {
            ++fail;
            if (shown < 20) {
                printf("FAIL ct=%d bd=%d %ux%u  %s\n", ct, bd, be32(d + 16), be32(d + 20), e.name.c_str());
                ++shown;
            }
        }
    }
    printf("\n==== PNG decode: %d total, %d OK, %d FAIL (palette OK=%d) ====\n", total, ok, fail, paletteOk);
    return 0;
}