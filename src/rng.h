// CraftEngine - Java 26.3 random streams.
//
// Ports, from the shipped bytecode (javap of the user's own 26.3 jar):
//   net.minecraft.world.level.levelgen.XoroshiroRandomSource
//   net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus
//   net.minecraft.world.level.levelgen.LegacyRandomSource / BitRandomSource
//   net.minecraft.world.level.levelgen.RandomSupport
//   net.minecraft.world.level.levelgen.MarsagliaPolarGaussian
//
// Java long/int overflow wraps; every operation below uses unsigned 64/32-bit
// arithmetic so the bit patterns match the JVM exactly.
#pragma once

#include <cstdint>

namespace mc {

uint64_t mixStafford13(uint64_t z);

// RandomSupport.upgradeSeedTo128bit: unmixed pair, then Stafford mixing.
void upgradeSeedTo128bit(int64_t seed, uint64_t& lo, uint64_t& hi);

// MarsagliaPolarGaussian.nextGaussian: shared by both streams.
struct MarsagliaPolarGaussian {
    double nextNext = 0.0;
    bool haveNext = false;
    void reset() { haveNext = false; }
    double nextGaussian(class RandomSource& source);
};

class RandomSource {
public:
    virtual ~RandomSource() = default;
    virtual void setSeed(int64_t seed) = 0;
    virtual int32_t nextInt() = 0;
    virtual int32_t nextInt(int32_t bound) = 0;
    virtual int64_t nextLong() = 0;
    virtual bool nextBoolean() = 0;
    virtual float nextFloat() = 0;
    virtual double nextDouble() = 0;
    virtual double nextGaussian() = 0;
    virtual void consumeCount(int32_t count) = 0;
    virtual RandomSource* fork() = 0;

    // RandomSource.nextInt(int, int), nextIntBetweenInclusive, triangle.
    int32_t nextInt(int32_t origin, int32_t bound);
    int32_t nextIntBetweenInclusive(int32_t min, int32_t max);
    float triangle(float min, float max);
    double triangle(double min, double max);
};

// Xoroshiro128PlusPlus.nextLong.
class Xoroshiro128PlusPlus {
public:
    Xoroshiro128PlusPlus() = default;
    Xoroshiro128PlusPlus(uint64_t lo, uint64_t hi);
    int64_t nextLong();
    uint64_t seedLo() const { return lo_; }
    uint64_t seedHi() const { return hi_; }

private:
    uint64_t lo_ = 0;
    uint64_t hi_ = 0;
};

class XoroshiroRandom : public RandomSource {
public:
    explicit XoroshiroRandom(int64_t seed);
    XoroshiroRandom(int64_t lo, int64_t hi);

    void setSeed(int64_t seed) override;
    int32_t nextInt() override;
    int32_t nextInt(int32_t bound) override;
    int64_t nextLong() override;
    bool nextBoolean() override;
    float nextFloat() override;
    double nextDouble() override;
    double nextGaussian() override;
    void consumeCount(int32_t count) override;
    RandomSource* fork() override;

    uint64_t bits(int count);

private:
    Xoroshiro128PlusPlus gen_;
    MarsagliaPolarGaussian gaussian_;
};

class LegacyRandom : public RandomSource {
public:
    explicit LegacyRandom(int64_t seed);

    void setSeed(int64_t seed) override;
    int32_t nextInt() override;
    int32_t nextInt(int32_t bound) override;
    int64_t nextLong() override;
    bool nextBoolean() override;
    float nextFloat() override;
    double nextDouble() override;
    double nextGaussian() override;
    void consumeCount(int32_t count) override;
    RandomSource* fork() override;

    // BitRandomSource.next(bits).
    int32_t next(int bits);

private:
    uint64_t seed_ = 0;
    MarsagliaPolarGaussian gaussian_;
};

} // namespace mc
