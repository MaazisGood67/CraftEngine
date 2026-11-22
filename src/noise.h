// CraftEngine - Minecraft 26.3 noise.
//
// Ported from the shipped bytecode:
//   net.minecraft.world.level.levelgen.synth.GradientNoise
//   net.minecraft.world.level.levelgen.synth.PerlinNoise
//   net.minecraft.world.level.levelgen.synth.NoiseStack (+Layer)
//   net.minecraft.world.level.levelgen.synth.NormalNoise (+Builder, OctaveInfo)
//
// Float/double distinctions are kept exactly: gradient dots and the octave sum
// are float in vanilla, coordinates and frequencies are double.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "rng.h"

namespace mc {

class Noise {
public:
    virtual ~Noise() = default;
    virtual float get(double x, double y, double z) const = 0;
    virtual float get(double x, double z) const { return get(x, 0.0, z); }
};

// GradientNoise.wrap - keeps the lattice inside the exact-representable range.
double wrapDouble(double value);

// GradientNoise$Gradient table, 16 entries (index 12-15 repeat 0-3/9/11).
struct Gradient {
    int x, y, z;
    float dot(float dx, float dy, float dz) const {
        return static_cast<float>(x) * dx + static_cast<float>(y) * dy +
               static_cast<float>(z) * dz;
    }
};
extern const Gradient kGradients[16];

// Mth.smoothstep / lerp / lerp2 / lerp3.
float smoothstep(float value);
float lerp(float delta, float start, float end);
float lerp2(float a, float b, float v0, float v1, float v2, float v3);
float lerp3(float a, float b, float c,
            float v0, float v1, float v2, float v3,
            float v4, float v5, float v6, float v7);

class PerlinNoise : public Noise {
public:
    explicit PerlinNoise(RandomSource& random);

    float get(double x, double y, double z) const override;
    float get(double x, double z) const override;

    double offsetX() const { return offsetX_; }
    double offsetY() const { return offsetY_; }
    double offsetZ() const { return offsetZ_; }

private:
    int permute(int index) const { return perms_[index & 255] & 255; }
    float sampleAndLerp(int x, int y, int z, float dx, float dy, float dz) const;

    uint8_t perms_[256];
    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
    double offsetZ_ = 0.0;
};

// NoiseStack: sum(amplitude * noise.get(x*frequency, y*frequency, z*frequency)).
class NoiseStack : public Noise {
public:
    struct Layer {
        Noise* noise = nullptr;
        double frequency = 0.0;
        float amplitude = 0.0f;
    };

    explicit NoiseStack(std::vector<Layer> layers);

    float get(double x, double y, double z) const override;
    float get(double x, double z) const override;

private:
    std::vector<Layer> layers_;
};

// NormalNoise: octave stack built from two positional factories, vanilla seeding.
class NormalNoise {
public:
    struct Parameters {
        double baseAmplitude = 1.0;
        int baseOctave = 0;
        int octaveCount = 1;
        bool normalize = true;
        std::vector<double> amplitudeModifiers;
    };

    explicit NormalNoise(const Parameters& parameters);

    // NormalNoise.create(RandomSource): returns a NoiseStack owned by the caller.
    std::unique_ptr<Noise> create(RandomSource& random) const;

    double normalizationFactor() const { return normalizationFactor_; }
    double rangeAmplitude() const { return rangeAmplitude_; }

    struct Octave {
        int index = 0;
        double frequency = 0.0;
        double amplitude = 0.0;
    };
    const std::vector<Octave>& octaves() const { return octaves_; }

private:
    Parameters parameters_;
    std::vector<Octave> octaves_;
    double normalizationFactor_ = 0.0;
    double rangeAmplitude_ = 0.0;
};

} // namespace mc
