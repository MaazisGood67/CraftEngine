// CraftEngine - MD5, needed by RandomSupport.seedFromHashOf (Guava MD5_128).
// Only used to derive per-octave noise seeds exactly like vanilla.
#pragma once
#include <cstdint>
#include <cstring>

namespace mc {
namespace md5 {

struct Ctx {
    uint32_t a = 0x67452301u;
    uint32_t b = 0xefcdab89u;
    uint32_t c = 0x98badcfeu;
    uint32_t d = 0x10325476u;
    uint64_t total = 0;
    uint8_t buf[64];
    size_t fill = 0;
};

inline uint32_t rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

inline void transform(uint32_t state[4], const uint8_t block[64]) {
    static const uint32_t K[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au,
        0xa8304613u, 0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
        0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u,
        0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u,
        0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
        0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
        0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u,
        0xffeff47du, 0x85845dd1u, 0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
        0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};
    static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                              5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

    uint32_t m[16];
    for (int i = 0; i < 16; ++i) {
        m[i] = static_cast<uint32_t>(block[i * 4]) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (int i = 0; i < 64; ++i) {
        uint32_t f;
        int g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }
        const uint32_t tmp = d;
        d = c;
        c = b;
        b = b + rotl(a + f + K[i] + m[g], S[i]);
        a = tmp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

inline void update(Ctx& ctx, const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    ctx.total += size;
    while (size > 0) {
        const size_t take = (size < 64 - ctx.fill) ? size : 64 - ctx.fill;
        std::memcpy(ctx.buf + ctx.fill, p, take);
        ctx.fill += take;
        p += take;
        size -= take;
        if (ctx.fill == 64) {
            uint32_t s[4] = {ctx.a, ctx.b, ctx.c, ctx.d};
            transform(s, ctx.buf);
            ctx.a = s[0]; ctx.b = s[1]; ctx.c = s[2]; ctx.d = s[3];
            ctx.fill = 0;
        }
    }
}

inline void digest(const void* data, size_t size, uint8_t out[16]) {
    Ctx ctx;
    update(ctx, data, size);
    const uint64_t bits = ctx.total * 8;
    uint8_t pad[72] = {0x80};
    size_t padLen = (ctx.fill < 56) ? (56 - ctx.fill) : (120 - ctx.fill);
    update(ctx, pad, padLen);
    uint8_t len[8];
    for (int i = 0; i < 8; ++i) len[i] = static_cast<uint8_t>(bits >> (8 * i));
    update(ctx, len, 8);
    const uint32_t s[4] = {ctx.a, ctx.b, ctx.c, ctx.d};
    for (int i = 0; i < 4; ++i) {
        out[i * 4] = static_cast<uint8_t>(s[i]);
        out[i * 4 + 1] = static_cast<uint8_t>(s[i] >> 8);
        out[i * 4 + 2] = static_cast<uint8_t>(s[i] >> 16);
        out[i * 4 + 3] = static_cast<uint8_t>(s[i] >> 24);
    }
}

} // namespace md5
} // namespace mc
