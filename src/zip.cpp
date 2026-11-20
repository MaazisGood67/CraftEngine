// CraftEngine - minimal in-memory ZIP reader (central directory + local headers + inflate).
#include "zip.h"

#include <zlib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kSigCEN = 0x02014b50u;
constexpr uint32_t kSigLOC = 0x04034b50u;
constexpr uint32_t kSigEOCD = 0x06054b50u;
constexpr uint32_t kSigZIP64EOCD = 0x06064b50u;
constexpr uint32_t kSigZIP64LOC = 0x07064b50u;

uint16_t rd16(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}
uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint64_t rd64(const uint8_t* p) {
    return (static_cast<uint64_t>(p[0]) | (static_cast<uint64_t>(p[1]) << 8) |
            (static_cast<uint64_t>(p[2]) << 16) | (static_cast<uint64_t>(p[3]) << 24) |
            (static_cast<uint64_t>(p[4]) << 32) | (static_cast<uint64_t>(p[5]) << 40) |
            (static_cast<uint64_t>(p[6]) << 48) | (static_cast<uint64_t>(p[7]) << 56));
}

bool Decompress(const uint8_t* in, size_t inSize, uint8_t* out, size_t outSize, bool& ok) {
    z_stream zs{};
    ok = false;
    if (inflateInit2(&zs, -15) != Z_OK) return false;
    zs.next_in = const_cast<Bytef*>(in);
    zs.avail_in = static_cast<uInt>(inSize);
    zs.next_out = const_cast<Bytef*>(out);
    zs.avail_out = static_cast<uInt>(outSize);
    const int ret = inflate(&zs, Z_FINISH);
    ok = (ret == Z_STREAM_END) ? true : false;
    inflateEnd(&zs);
    return true;
}

} // namespace

bool LoadZip(const std::string& path, std::vector<ZipEntry>& out, std::string& err) {
    out.clear();

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { err = "cannot open zip file"; return false; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); err = "seek failed"; return false; }
    const long fsz = ftell(f);
    if (fsz < 0) { fclose(f); err = "size failed"; return false; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); err = "seek failed"; return false; }
    std::vector<uint8_t> file(static_cast<size_t>(fsz));
    if (fsz > 0 && fread(file.data(), 1, file.size(), f) != file.size()) {
        fclose(f);
        err = "read failed";
        return false;
    }
    fclose(f);

    const size_t fileSize = file.size();
    if (fileSize < 22) { err = "zip too small"; return false; }

    size_t eocdPos = 0;
    bool found = false;
    const size_t scanStart = (fileSize > 65557) ? (fileSize - 65557) : 0;
    size_t iPos = fileSize - 22;
    while (iPos >= scanStart) {
        if (rd32(&file[iPos]) == kSigEOCD) { eocdPos = iPos; found = true; break; }
        if (iPos == scanStart) break;
        --iPos;
    }
    if (!found) { err = "EOCD not found"; return false; }

    uint64_t totalEntries = rd16(&file[eocdPos + 10]);
    uint64_t cdSize = rd32(&file[eocdPos + 12]);
    uint64_t cdOffset = rd32(&file[eocdPos + 16]);

    const bool needZip64 = (rd16(&file[eocdPos + 10]) == 0xFFFFu) || (rd32(&file[eocdPos + 12]) == 0xFFFFFFFFu) ||
                            (rd32(&file[eocdPos + 16]) == 0xFFFFFFFFu);
    if (needZip64) {
        size_t locPos = 0;
        bool lfound = false;
        size_t li = (eocdPos >= 20) ? (eocdPos - 20) : 0;
        while (true) {
            if (rd32(&file[li]) == kSigZIP64LOC) { locPos = li; lfound = true; break; }
            if (li == 0) break;
            --li;
        }
        if (lfound) {
            const uint64_t z64Off = rd64(&file[locPos + 8]);
            if (z64Off + 56 <= fileSize && rd32(&file[static_cast<size_t>(z64Off)]) == kSigZIP64EOCD) {
                totalEntries = rd64(&file[static_cast<size_t>(z64Off) + 32]);
                cdSize = rd64(&file[static_cast<size_t>(z64Off) + 40]);
                cdOffset = rd64(&file[static_cast<size_t>(z64Off) + 48]);
            }
        }
    }

    size_t p = static_cast<size_t>(cdOffset);
    size_t end = (static_cast<size_t>(cdOffset) + static_cast<size_t>(cdSize) < fileSize) ? (static_cast<size_t>(cdOffset) + static_cast<size_t>(cdSize)) : fileSize;

    for (uint32_t i = 0; i < totalEntries; ++i) {
        if (p + 46 > end || rd32(&file[p]) != kSigCEN) { err = "bad central directory"; return false; }
        const uint16_t nameLen = rd16(&file[p + 28]);
        const uint16_t extraLen = rd16(&file[p + 30]);
        const uint16_t commentLen = rd16(&file[p + 32]);
        const uint32_t compSize = rd32(&file[p + 20]);
        const uint32_t uncompSize = rd32(&file[p + 24]);
        const uint32_t localOff = rd32(&file[p + 42]);
        const uint16_t method = rd16(&file[p + 10]);

        std::string name;
        name.reserve(nameLen);
        for (uint16_t k = 0; k < nameLen; ++k) {
            char c = static_cast<char>(file[p + 46 + k]);
            if (c == '\\') c = '/';
            name.push_back(c);
        }

        p += 46 + static_cast<size_t>(nameLen) + extraLen + commentLen;

        if (!name.empty() && name[name.size() - 1] == '/') continue; // directory entry

        if (localOff + 30 > fileSize) break;
        if (rd32(&file[localOff]) != kSigLOC) break;

        const uint16_t lNameLen = rd16(&file[localOff + 26]);
        const uint16_t lExtraLen = rd16(&file[localOff + 28]);
        const size_t dataOff = static_cast<size_t>(localOff) + 30 + lNameLen + lExtraLen;
        const size_t avail = static_cast<size_t>(compSize);
        if (dataOff + avail > fileSize) break;

        ZipEntry e;
        e.name = name;
        if (avail == 0) {
            e.data.clear();
            out.push_back(std::move(e));
            continue;
        }
        if (method == 0) {
            e.data.assign(&file[dataOff], &file[dataOff + avail]);
        } else if (method == 8) {
            e.data.resize(uncompSize);
            bool ok = false;
            Decompress(&file[dataOff], avail, e.data.data(), e.data.size(), ok);
            if (!ok) { continue; } // skip bad entries (e.g. odd metadata blobs)
        } else {
            continue;
        }
        out.push_back(std::move(e));
    }

    return true;
}
