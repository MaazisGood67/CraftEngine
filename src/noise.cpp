#include "noise.h"

#include <cmath>
#include <string>

namespace mc {

namespace {
// GradientNoise.HALF_ROUND_OFF == Math.nextDown(16777216.0)
constexpr double kHalfRoundOff = 16777215.999999998;
constexpr double kRoundOff = 33554432.0; // 2^25
constexpr double kInputFactor = 1.0181268882175227;
constexpr double kTargetDeviation = 0.3333333333333333;
constexpr double kOctaveDeviation = 0.2702247831245211;

inline double JavaFloor(double v) { return std::floor(v); }

// Mth.floor(double) -> int (Java cast semantics after floor).
inline int FloorToInt(double v) { return static_cast<int>(std::floor(v)); }
} // namespace

double wrapDouble(double value) {
    if (value >= -kHalfRoundOff && value < kHalfRoundOff) return value;
    return value - JavaFloor(value / kRoundOff + 0.5) * kRoundOff;
}

const Gradient kGradients[16] = {
    {1, 1, 0},  {-1, 1, 0},  {1, -1, 0},  {-1, -1, 0},
    {1, 0, 1},  {-1, 0, 1},  {1, 0, -1},  {-1, 0, -1},
    {0, 1, 1},  {0, -1, 1},  {0, 1, -1},  {0, -1, -1},
    {1, 1, 0},  {0, -1, 1},  {-1, 1, 0},  {0, -1, -1},
};

float smoothstep(float value) {
    return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
}

float lerp(float delta, float start, float end) {
    return start + delta * (end - start);
}

float lerp2(float a, float b, float v0, float v1, float v2, float v3) {
    return lerp(b, lerp(a, v0, v1), lerp(a, v2, v3));
}

float lerp3(float a, float b, float c,
            float v0, float v1, float v2, float v3,
            float v4, float v5, float v6, float v7) {
    return lerp(c, lerp2(a, b, v0, v1, v2, v3), lerp2(a, b, v4, v5, v6, v7));
}

// ---------------------------------------------------------------------------
// PerlinNoise (extends GradientNoise in 26.3)
// ---------------------------------------------------------------------------
PerlinNoise::PerlinNoise(RandomSource& random) {
    offsetX_ = random.nextDouble() * 256.0;
    offsetY_ = random.nextDouble() * 256.0;
    offsetZ_ = random.nextDouble() * 256.0;

    for (int i = 0; i < 256; ++i) perms_[i] = static_cast<uint8_t>(i);
    for (int i = 0; i < 256; ++i) {
        const int j = random.nextInt(256 - i);
        const uint8_t t = perms_[i];
        perms_[i] = perms_[i + j];
        perms_[i + j] = t;
    }
}

float PerlinNoise::get(double x, double z) const {
    return get(x, 0.0, z);
}

float PerlinNoise::get(double x, double y, double z) const {
    const double dx = wrapDouble(x) + offsetX_;
    const double dy = wrapDouble(y) + offsetY_;
    const double dz = wrapDouble(z) + offsetZ_;
    const int ix = FloorToInt(dx);
    const int iy = FloorToInt(dy);
    const int iz = FloorToInt(dz);
    const float fx = static_cast<float>(dx - static_cast<double>(ix));
    const float fy = static_cast<float>(dy - static_cast<double>(iy));
    const float fz = static_cast<float>(dz - static_cast<double>(iz));
    return sampleAndLerp(ix, iy, iz, fx, fy, fz);
}

float PerlinNoise::sampleAndLerp(int x, int y, int z, float dx, float dy, float dz) const {
    const int p0 = permute(x);
    const int p1 = permute(x + 1);
    const int p00 = permute(p0 + y);
    const int p10 = permute(p1 + y);
    const int p01 = permute(p0 + y + 1);
    const int p11 = permute(p1 + y + 1);

    const float g000 = kGradients[permute(p00 + z) & 15].dot(dx, dy, dz);
    const float g100 = kGradients[permute(p10 + z) & 15].dot(dx - 1.0f, dy, dz);
    const float g010 = kGradients[permute(p01 + z) & 15].dot(dx, dy - 1.0f, dz);
    const float g110 = kGradients[permute(p11 + z) & 15].dot(dx - 1.0f, dy - 1.0f, dz);

    const float g001 = kGradients[permute(p00 + z + 1) & 15].dot(dx, dy, dz - 1.0f);
    const float g101 = kGradients[permute(p10 + z + 1) & 15].dot(dx - 1.0f, dy, dz - 1.0f);
    const float g011 = kGradients[permute(p01 + z + 1) & 15].dot(dx, dy - 1.0f, dz - 1.0f);
    const float g111 = kGradients[permute(p11 + z + 1) & 15].dot(
        dx - 1.0f, dy - 1.0f, dz - 1.0f);

    const float sx = smoothstep(dx);
    const float sy = smoothstep(dy);
    const float sz = smoothstep(dz);

    return lerp3(sx, sy, sz, g000, g100, g010, g110, g001, g101, g011, g111);
}

// ---------------------------------------------------------------------------
// NoiseStack
// ---------------------------------------------------------------------------
NoiseStack::NoiseStack(std::vector<Layer> layers) : layers_(std::move(layers)) {}

float NoiseStack::get(double x, double y, double z) const {
    float sum = 0.0f;
    for (const Layer& layer : layers_) {
        const double f = layer.frequency;
        sum += layer.amplitude * layer.noise->get(x * f, y * f, z * f);
    }
    return sum;
}

float NoiseStack::get(double x, double z) const {
    float sum = 0.0f;
    for (const Layer& layer : layers_) {
        const double f = layer.frequency;
        sum += layer.amplitude * layer.noise->get(x * f, z * f);
    }
    return sum;
}

// ---------------------------------------------------------------------------
// NormalNoise
// ---------------------------------------------------------------------------
namespace {

double amplitudeModifier(const std::vector<double>& modifiers, int index) {
    if (modifiers.empty()) return 1.0;
    return modifiers[static_cast<size_t>(index)];
}

double estimateDeviation(const std::vector<NormalNoise::Octave>& octaves) {
    double sumSq = 0.0;
    for (const NormalNoise::Octave& octave : octaves) {
        const double amp = kOctaveDeviation * std::fabs(octave.amplitude);
        sumSq += amp * amp;
    }
    return std::sqrt(sumSq);
}

double computeNormalizationFactor(double sumAmplitude,
                                  const std::vector<NormalNoise::Octave>& octaves) {
    const double deviation = estimateDeviation(octaves);
    if (deviation == 0.0) return 0.0;
    const double doubled = deviation * std::sqrt(2.0);
    return (sumAmplitude * kTargetDeviation) / doubled;
}

} // namespace

NormalNoise::NormalNoise(const Parameters& parameters) : parameters_(parameters) {
    // buildOctaves(baseOctave, baseAmplitude, octaveCount, normalize, modifiers)
    double frequency = std::pow(2.0, static_cast<double>(parameters.baseOctave));
    double amplitude = parameters.baseAmplitude;
    if (parameters.normalize) {
        amplitude *= std::pow(0.5, static_cast<double>(-(parameters.octaveCount - 1))) /
                     (std::pow(0.5, static_cast<double>(-parameters.octaveCount)) - 1.0);
    }

    for (int i = 0; i < parameters.octaveCount; ++i) {
        const double modifier = amplitudeModifier(parameters.amplitudeModifiers, i);
        if (modifier != 0.0) {
            octaves_.push_back({parameters.baseOctave + i, frequency, amplitude * modifier});
        }
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    double sumAmplitude = 0.0;
    for (const Octave& octave : octaves_) sumAmplitude += octave.amplitude;

    normalizationFactor_ = computeNormalizationFactor(sumAmplitude, octaves_);
    rangeAmplitude_ = sumAmplitude * kTargetDeviation * 6.0;
}

std::unique_ptr<Noise> NormalNoise::create(RandomSource& random) const {
    // Both factories are forked before any octave is built (bytecode order).
    std::unique_ptr<PositionalRandomFactory> factory1(random.forkPositional());
    std::unique_ptr<PositionalRandomFactory> factory2(random.forkPositional());

    std::vector<NoiseStack::Layer> layers;
    std::vector<std::unique_ptr<Noise>> owned;
    layers.reserve(octaves_.size() * 2);
    owned.reserve(octaves_.size() * 2);

    for (const Octave& octave : octaves_) {
        const std::string seed = "octave_" + std::to_string(octave.index);

        std::unique_ptr<RandomSource> random1(factory1->fromHashOf(seed));
        std::unique_ptr<RandomSource> random2(factory2->fromHashOf(seed));
        auto perlin1 = std::make_unique<PerlinNoise>(*random1);
        auto perlin2 = std::make_unique<PerlinNoise>(*random2);

        const float scaledAmplitude =
            static_cast<float>(normalizationFactor_ * octave.amplitude);

        layers.push_back({perlin1.get(), octave.frequency, scaledAmplitude});
        layers.push_back({perlin2.get(), octave.frequency * kInputFactor, scaledAmplitude});
        owned.push_back(std::move(perlin1));
        owned.push_back(std::move(perlin2));
    }

    // NoiseStack only reads the Perlin pointers, so hand the ownership to a
    // small wrapper that keeps them alive for the stack's lifetime.
    struct OwnedStack final : Noise {
        std::vector<std::unique_ptr<Noise>> owned;
        std::unique_ptr<NoiseStack> stack;
        float get(double x, double y, double z) const override { return stack->get(x, y, z); }
        float get(double x, double z) const override { return stack->get(x, z); }
    };

    auto result = std::make_unique<OwnedStack>();
    result->stack = std::make_unique<NoiseStack>(std::move(layers));
    result->owned = std::move(owned);
    return result;
}

} // namespace mc
