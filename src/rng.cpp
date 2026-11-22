#include "rng.h"

#include <cmath>
#include <string>

#include "md5.h"

namespace mc {

namespace {
constexpr uint64_t kMask48 = (1ull << 48) - 1;
constexpr uint64_t kMultiplier = 25214903917ull;
constexpr uint64_t kIncrement = 11ull;
constexpr double kDoubleUnit = 1.1102230246251565e-16; // 2^-53
constexpr float kFloatUnit = 5.9604645e-8f;            // 2^-24

inline uint64_t Rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

class XoroshiroPositionalFactory final : public PositionalRandomFactory {
public:
    XoroshiroPositionalFactory(uint64_t lo, uint64_t hi) : lo_(lo), hi_(hi) {}

    RandomSource* at(int32_t x, int32_t y, int32_t z) override {
        const int64_t seed = mthGetSeed(x, y, z) ^ static_cast<int64_t>(lo_);
        return new XoroshiroRandom(seed, static_cast<int64_t>(hi_));
    }

    RandomSource* fromHashOf(const std::string& name) override {
        const Seed128bit s = seedFromHashOf(name).xorWith(lo_, hi_);
        return new XoroshiroRandom(static_cast<int64_t>(s.lo), static_cast<int64_t>(s.hi));
    }

    RandomSource* fromSeed(int64_t seed) override {
        return new XoroshiroRandom(seed ^ static_cast<int64_t>(lo_),
                                   seed ^ static_cast<int64_t>(hi_));
    }

private:
    uint64_t lo_;
    uint64_t hi_;
};

class LegacyPositionalFactory final : public PositionalRandomFactory {
public:
    explicit LegacyPositionalFactory(int64_t seed) : seed_(static_cast<uint64_t>(seed)) {}

    RandomSource* at(int32_t x, int32_t y, int32_t z) override {
        return new LegacyRandom(mthGetSeed(x, y, z) ^ static_cast<int64_t>(seed_));
    }

    RandomSource* fromHashOf(const std::string& name) override {
        // LegacyPositionalRandomFactory uses String.hashCode(), not MD5.
        int32_t hash = 0;
        for (unsigned char c : name) hash = 31 * hash + static_cast<int32_t>(c);
        return new LegacyRandom(static_cast<int64_t>(hash) ^ static_cast<int64_t>(seed_));
    }

    RandomSource* fromSeed(int64_t seed) override { return new LegacyRandom(seed); }

private:
    uint64_t seed_;
};
} // namespace

Seed128bit seedFromHashOf(const std::string& name) {
    uint8_t hash[16];
    md5::digest(name.data(), name.size(), hash);
    auto bigEndian = [](const uint8_t* p) {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
        return v;
    };
    return Seed128bit(bigEndian(hash), bigEndian(hash + 8));
}

int64_t mthGetSeed(int32_t x, int32_t y, int32_t z) {
    // Mth.getSeed: (x*3129871) ^ (z*116129781L) ^ y, then a mix, then >> 16.
    const int32_t xTerm = static_cast<int32_t>(static_cast<uint32_t>(x) * 3129871u);
    int64_t l = static_cast<int64_t>(xTerm) ^
                (static_cast<int64_t>(z) * 116129781LL) ^
                static_cast<int64_t>(y);
    l = l * l * 42317861LL + l * 11LL;
    return l >> 16;
}

uint64_t mixStafford13(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull; // -4658895280553007687
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull; // -7723592293110705685
    return z ^ (z >> 31);
}

void upgradeSeedTo128bit(int64_t seed, uint64_t& lo, uint64_t& hi) {
    const uint64_t s = static_cast<uint64_t>(seed);
    const uint64_t unmixedLo = s ^ 7640891576956012809ull;
    const uint64_t unmixedHi = unmixedLo + 0x9e3779b97f4a7c15ull; // -7046029254386353131
    lo = mixStafford13(unmixedLo);
    hi = mixStafford13(unmixedHi);
}

double MarsagliaPolarGaussian::nextGaussian(RandomSource& source) {
    if (haveNext) {
        haveNext = false;
        return nextNext;
    }
    double x, y, r;
    do {
        x = 2.0 * source.nextDouble() - 1.0;
        y = 2.0 * source.nextDouble() - 1.0;
        r = x * x + y * y;
    } while (r >= 1.0 || r == 0.0);
    const double f = std::sqrt(-2.0 * std::log(r) / r);
    nextNext = y * f;
    haveNext = true;
    return x * f;
}

int32_t RandomSource::nextInt(int32_t origin, int32_t bound) {
    if (origin >= bound) return origin; // Java throws; callers stay in range.
    return origin + nextInt(bound - origin);
}

int32_t RandomSource::nextIntBetweenInclusive(int32_t min, int32_t max) {
    return nextInt(max - min + 1) + min;
}

float RandomSource::triangle(float min, float max) {
    const float a = nextFloat();
    const float b = nextFloat();
    return min + (max - min) * (a - b);
}

double RandomSource::triangle(double min, double max) {
    const double a = nextDouble();
    const double b = nextDouble();
    return min + (max - min) * (a - b);
}

// ---------------------------------------------------------------------------
// Xoroshiro128++
// ---------------------------------------------------------------------------
Xoroshiro128PlusPlus::Xoroshiro128PlusPlus(uint64_t lo, uint64_t hi) : lo_(lo), hi_(hi) {
    if ((lo_ | hi_) == 0) {
        lo_ = 0x9e3779b97f4a7c15ull; // -7046029254386353131
        hi_ = 0x6a09e667f3bcc909ull; // 7640891576956012809
    }
}

int64_t Xoroshiro128PlusPlus::nextLong() {
    const uint64_t s0 = lo_;
    const uint64_t s1 = hi_;
    const uint64_t result = Rotl(s0 + s1, 17) + s0;
    hi_ = s1 ^ s0;
    lo_ = Rotl(s0, 49) ^ hi_ ^ (hi_ << 21);
    hi_ = Rotl(hi_, 28);
    return static_cast<int64_t>(result);
}

// ---------------------------------------------------------------------------
// XoroshiroRandomSource
// ---------------------------------------------------------------------------
XoroshiroRandom::XoroshiroRandom(int64_t seed) {
    setSeed(seed);
}

XoroshiroRandom::XoroshiroRandom(int64_t lo, int64_t hi)
    : gen_(static_cast<uint64_t>(lo), static_cast<uint64_t>(hi)) {}

void XoroshiroRandom::setSeed(int64_t seed) {
    uint64_t lo = 0, hi = 0;
    upgradeSeedTo128bit(seed, lo, hi);
    gen_ = Xoroshiro128PlusPlus(lo, hi);
    gaussian_.reset();
}

int32_t XoroshiroRandom::nextInt() {
    return static_cast<int32_t>(static_cast<uint32_t>(gen_.nextLong()));
}

int32_t XoroshiroRandom::nextInt(int32_t bound) {
    if (bound <= 0) return 0;
    uint64_t l = static_cast<uint32_t>(nextInt());
    uint64_t m = l * static_cast<uint64_t>(static_cast<int64_t>(bound));
    uint32_t n = static_cast<uint32_t>(m);
    if (n < static_cast<uint32_t>(bound)) {
        const uint32_t i = (0u - static_cast<uint32_t>(bound)) % static_cast<uint32_t>(bound);
        while (n < i) {
            l = static_cast<uint32_t>(nextInt());
            m = l * static_cast<uint64_t>(static_cast<int64_t>(bound));
            n = static_cast<uint32_t>(m);
        }
    }
    return static_cast<int32_t>(static_cast<int64_t>(m) >> 32);
}

int64_t XoroshiroRandom::nextLong() {
    return gen_.nextLong();
}

bool XoroshiroRandom::nextBoolean() {
    return (static_cast<uint64_t>(gen_.nextLong()) & 1ull) != 0;
}

uint64_t XoroshiroRandom::bits(int count) {
    return static_cast<uint64_t>(gen_.nextLong()) >> (64 - count);
}

float XoroshiroRandom::nextFloat() {
    return static_cast<float>(bits(24)) * kFloatUnit;
}

double XoroshiroRandom::nextDouble() {
    return static_cast<double>(bits(53)) * kDoubleUnit;
}

double XoroshiroRandom::nextGaussian() {
    return gaussian_.nextGaussian(*this);
}

void XoroshiroRandom::consumeCount(int32_t count) {
    for (int32_t i = 0; i < count; ++i) gen_.nextLong();
}

RandomSource* XoroshiroRandom::fork() {
    const int64_t lo = gen_.nextLong();
    const int64_t hi = gen_.nextLong();
    return new XoroshiroRandom(lo, hi);
}

PositionalRandomFactory* XoroshiroRandom::forkPositional() {
    const uint64_t lo = static_cast<uint64_t>(gen_.nextLong());
    const uint64_t hi = static_cast<uint64_t>(gen_.nextLong());
    return new XoroshiroPositionalFactory(lo, hi);
}

// ---------------------------------------------------------------------------
// LegacyRandomSource / BitRandomSource
// ---------------------------------------------------------------------------
LegacyRandom::LegacyRandom(int64_t seed) {
    setSeed(seed);
}

void LegacyRandom::setSeed(int64_t seed) {
    seed_ = (static_cast<uint64_t>(seed) ^ kMultiplier) & kMask48;
    gaussian_.reset();
}

int32_t LegacyRandom::next(int bitsCount) {
    seed_ = (seed_ * kMultiplier + kIncrement) & kMask48;
    return static_cast<int32_t>(static_cast<uint32_t>(seed_ >> (48 - bitsCount)));
}

int32_t LegacyRandom::nextInt() {
    return next(32);
}

int32_t LegacyRandom::nextInt(int32_t bound) {
    if (bound <= 0) return 0;
    // BitRandomSource.nextInt(int): power-of-two fast path, then rejection.
    if ((bound & -bound) == bound) {
        const int64_t product = static_cast<int64_t>(bound) * static_cast<int64_t>(next(31));
        return static_cast<int32_t>(product >> 31);
    }
    int32_t bits, val;
    do {
        bits = next(31);
        val = bits % bound;
        const uint32_t sum = static_cast<uint32_t>(bits) - static_cast<uint32_t>(val) +
                             static_cast<uint32_t>(bound - 1);
        if (static_cast<int32_t>(sum) >= 0) break;
    } while (true);
    return val;
}

int64_t LegacyRandom::nextLong() {
    const int64_t hi = static_cast<int64_t>(next(32));
    const int64_t lo = static_cast<int64_t>(next(32));
    return (hi << 32) + lo;
}

bool LegacyRandom::nextBoolean() {
    return next(1) != 0;
}

float LegacyRandom::nextFloat() {
    return static_cast<float>(next(24)) * kFloatUnit;
}

double LegacyRandom::nextDouble() {
    const int64_t hi = static_cast<int64_t>(next(26));
    const int64_t lo = static_cast<int64_t>(next(27));
    return static_cast<double>((hi << 27) + lo) * kDoubleUnit;
}

double LegacyRandom::nextGaussian() {
    return gaussian_.nextGaussian(*this);
}

void LegacyRandom::consumeCount(int32_t count) {
    for (int32_t i = 0; i < count; ++i) nextInt();
}

RandomSource* LegacyRandom::fork() {
    return new LegacyRandom(nextLong());
}

PositionalRandomFactory* LegacyRandom::forkPositional() {
    return new LegacyPositionalFactory(nextLong());
}

} // namespace mc
