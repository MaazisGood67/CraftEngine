// CraftEngine probe: prints the C++ side of a differential harness.
//
// The Java oracle (tools/oracle/*.java) runs the same script against the real
// 26.3 jar. tools/parity.ps1 compares the two JSON files.
//
// Build: tools/build-probe.ps1   Run: CraftEngineProbe.exe --probe rng --out <path>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
using namespace std::string_literals;
#include "df.h"
#include "json.h"
#include "noise.h"
#include "rng.h"

namespace {

const int64_t kSeeds[] = {
    0, 1, 42, 1337, -1,
    9223372036854775807LL, (-9223372036854775807LL - 1),
    1234567890123456789LL, -6148914691236517206LL,
};

struct Entry {
    std::string gen;
    int64_t seed;
    std::string op;
    std::vector<std::string> values;
};

std::vector<Entry> g_entries;

std::string Bits(float f) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    return std::to_string(bits);
}

std::string Bits(double d) {
    uint64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(bits));
    return std::to_string(bits);
}

void Add(const std::string& gen, int64_t seed, const std::string& op,
         const std::vector<std::string>& values) {
    g_entries.push_back({gen, seed, op, values});
}

void Run(const std::string& gen, mc::RandomSource& r, int64_t seed) {
    {
        std::vector<std::string> v;
        for (int i = 0; i < 4; ++i) v.push_back(std::to_string(r.nextInt()));
        Add(gen, seed, "nextInt", v);
    }
    const int32_t bounds[] = {16, 1000, 1, 2, 3, 7, 64, 2147483647};
    for (int32_t bound : bounds) {
        std::vector<std::string> v;
        for (int i = 0; i < 4; ++i) v.push_back(std::to_string(r.nextInt(bound)));
        Add(gen, seed, "nextInt(" + std::to_string(bound) + ")", v);
    }
    {
        std::vector<std::string> v;
        for (int i = 0; i < 3; ++i) v.push_back(std::to_string(r.nextLong()));
        Add(gen, seed, "nextLong", v);
    }
    {
        std::vector<std::string> v;
        for (int i = 0; i < 3; ++i) v.push_back(r.nextBoolean() ? "true" : "false");
        Add(gen, seed, "nextBoolean", v);
    }
    {
        std::vector<std::string> v;
        for (int i = 0; i < 3; ++i) v.push_back(Bits(r.nextFloat()));
        Add(gen, seed, "nextFloatBits", v);
    }
    {
        std::vector<std::string> v;
        for (int i = 0; i < 3; ++i) v.push_back(Bits(r.nextDouble()));
        Add(gen, seed, "nextDoubleBits", v);
    }
    {
        std::vector<std::string> v;
        r.consumeCount(3);
        v.push_back(std::to_string(r.nextLong()));
        Add(gen, seed, "consume3Long", v);
    }
    {
        std::vector<std::string> v;
        for (int i = 0; i < 3; ++i) v.push_back(std::to_string(r.nextIntBetweenInclusive(-5, 5)));
        Add(gen, seed, "nextIntBetweenInclusive(-5,5)", v);
    }
    {
        std::vector<std::string> v;
        for (int i = 0; i < 2; ++i) v.push_back(Bits(r.triangle(0.0f, 1.0f)));
        Add(gen, seed, "triangleFloatBits", v);
    }
    {
        std::vector<std::string> v;
        for (int i = 0; i < 2; ++i) v.push_back(Bits(r.triangle(0.0, 1.0)));
        Add(gen, seed, "triangleDoubleBits", v);
    }
    {
        mc::RandomSource* forked = r.fork();
        std::vector<std::string> v;
        v.push_back(std::to_string(forked->nextLong()));
        delete forked;
        Add(gen, seed, "forkLong", v);
    }
}

struct NoiseEntry {
    std::string name;
    int64_t seed;
    std::vector<std::string> values;
};

std::vector<NoiseEntry> g_noise;

void AddNoise(const std::string& name, int64_t seed, const std::vector<std::string>& values) {
    g_noise.push_back({name, seed, values});
}

void Point3d(int i, double& x, double& y, double& z) {
    x = (i % 8) * 37.5 - 100.0;
    y = ((i * 7) % 37) * 3.0 - 64.0;
    z = (i / 8) * 23.25 + 50.0;
}

void Point2d(int i, double& x, double& z) {
    x = i * 3.7 - 50.0;
    z = i * 2.9 + 7.0;
}

bool ReadFile(const std::filesystem::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool LoadNoiseParameters(const std::filesystem::path& path, mc::NormalNoise::Parameters& p) {
    std::string text;
    if (!ReadFile(path, text)) return false;
    Json root;
    if (!ParseJson(text, root)) return false;
    if (const Json* v = root.find("base_amplitude")) p.baseAmplitude = v->num;
    if (const Json* v = root.find("base_octave")) p.baseOctave = static_cast<int>(v->num);
    if (const Json* v = root.find("octave_count")) p.octaveCount = static_cast<int>(v->num);
    if (const Json* mods = root.find("amplitude_modifiers")) {
        for (const Json& m : mods->arr) p.amplitudeModifiers.push_back(m.num);
    }
    return true;
}

void ProbeNoise(const std::string& outPath, const std::filesystem::path& noiseDir) {
    const int64_t seeds[] = {0, 42, 1337, -1};

    // PerlinNoise direct: offsets and samples expose permutation/offset bugs.
    for (int64_t seed : seeds) {
        mc::XoroshiroRandom random(seed);
        mc::PerlinNoise perlin(random);

        std::vector<std::string> offsets;
        offsets.push_back(Bits(perlin.offsetX()));
        offsets.push_back(Bits(perlin.offsetY()));
        offsets.push_back(Bits(perlin.offsetZ()));
        AddNoise("perlin-offsets:" + std::to_string(seed), seed, offsets);

        std::vector<std::string> values;
        for (int i = 0; i < 96; ++i) {
            double x, y, z;
            Point3d(i, x, y, z);
            values.push_back(Bits(perlin.get(x, y, z)));
        }
        AddNoise("perlin3d:" + std::to_string(seed), seed, values);

        values.clear();
        for (int i = 0; i < 64; ++i) {
            double x, z;
            Point2d(i, x, z);
            values.push_back(Bits(perlin.get(x, 0.0, z)));
        }
        AddNoise("perlin2d:" + std::to_string(seed), seed, values);
    }

    const char* names[] = {
        "continentalness", "erosion", "ridge", "temperature", "vegetation",
        "cave_cheese", "aquifer_barrier", "surface", "jagged", "noodle",
    };
    for (const char* name : names) {
        mc::NormalNoise::Parameters params;
        const std::filesystem::path file = noiseDir / (std::string(name) + ".json");
        if (!LoadNoiseParameters(file, params)) {
            std::fprintf(stderr, "cannot read %s\n", file.string().c_str());
            continue;
        }
        mc::NormalNoise normal(params);
        for (int64_t seed : seeds) {
            mc::XoroshiroRandom random(seed);
            std::unique_ptr<mc::Noise> noise = normal.create(random);
            std::vector<std::string> values;
            for (int i = 0; i < 128; ++i) {
                double x, y, z;
                Point3d(i, x, y, z);
                values.push_back(Bits(noise->get(x, y, z)));
            }
            AddNoise("normal3d:" + std::string(name) + ":" + std::to_string(seed), seed, values);
        }
    }

    std::filesystem::path p(outPath);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    FILE* f = fopen(outPath.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
        return;
    }
    std::fprintf(f, "{\n  \"version\":\"26.3\",\n  \"noise\":[\n");
    for (size_t i = 0; i < g_noise.size(); ++i) {
        const NoiseEntry& e = g_noise[i];
        std::fprintf(f, "    {\"name\":\"%s\",\"seed\":\"%lld\",\"values\":[",
                     e.name.c_str(), static_cast<long long>(e.seed));
        for (size_t k = 0; k < e.values.size(); ++k) {
            std::fprintf(f, "%s\"%s\"", k ? "," : "", e.values[k].c_str());
        }
        std::fprintf(f, "]}%s\n", i + 1 == g_noise.size() ? "" : ",");
    }
    std::fprintf(f, "  ]\n}\n");
    fclose(f);
    std::printf("wrote %s (%d entries)\n", outPath.c_str(), static_cast<int>(g_noise.size()));
}

void ProbeDf(const std::string& outPath, const std::filesystem::path& dfDir,
             const std::filesystem::path& noiseDir) {
    mc::DfInterp interp(dfDir, noiseDir);
    const std::filesystem::path settingsPath = dfDir / "overworld.json";
    std::string text;
    if (!ReadFile(settingsPath, text)) {
        std::fprintf(stderr, "cannot read %s\n", settingsPath.string().c_str());
        return;
    }
    Json root;
    ParseJson(text, root);
    const char* names[] = {
        "overworld/chunk_surface_level", "overworld/continents", "overworld/depth",
        "overworld/erosion", "overworld/final_density", "overworld/ridges",
        "overworld/temperature", "overworld/vegetation",
    };
    std::vector<std::string> keys(names, names + 8);
    interp.run(outPath, keys);
}

// Debug helper: dump the shared Noise instances at one point (compare with
// tools/oracle/DfDebug.java "noises" mode to attribute a mismatch).
void ProbeNoises(const std::string& outPath, const std::filesystem::path& dfDir,
                 const std::filesystem::path& noiseDir) {
    mc::DfInterp interp(dfDir, noiseDir);
    const char* names[] = {
        "overworld/chunk_surface_level", "overworld/continents", "overworld/depth",
        "overworld/erosion", "overworld/final_density", "overworld/ridges",
        "overworld/temperature", "overworld/vegetation",
    };
    std::vector<std::string> keys(names, names + 8);
    interp.dumpNoises(outPath, keys, 0.0, 0.0, 0.0);
}

void ProbeRng(const std::string& outPath) {
    for (int64_t seed : kSeeds) {
        mc::XoroshiroRandom xoroshiro(seed);
        Run("xoroshiro", xoroshiro, seed);
        mc::LegacyRandom legacy(seed);
        Run("legacy", legacy, seed);
    }

    std::filesystem::path p(outPath);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    FILE* f = fopen(outPath.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
        return;
    }
    std::fprintf(f, "{\n  \"version\":\"26.3\",\n  \"rng\":[\n");
    for (size_t i = 0; i < g_entries.size(); ++i) {
        const Entry& e = g_entries[i];
        std::fprintf(f, "    {\"gen\":\"%s\",\"seed\":\"%lld\",\"op\":\"%s\",\"values\":[",
                     e.gen.c_str(), static_cast<long long>(e.seed), e.op.c_str());
        for (size_t k = 0; k < e.values.size(); ++k) {
            std::fprintf(f, "%s\"%s\"", k ? "," : "", e.values[k].c_str());
        }
        std::fprintf(f, "]}%s\n", i + 1 == g_entries.size() ? "" : ",");
    }
    std::fprintf(f, "  ]\n}\n");
    fclose(f);
    std::printf("wrote %s (%d entries)\n", outPath.c_str(), static_cast<int>(g_entries.size()));
}

} // namespace

int main(int argc, char** argv) {
    std::string probe = "rng";
    std::string out; // defaults to .local/probe/<probe>.json once --probe is known
    std::string noiseDir = ".local/noise";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--probe") == 0 && i + 1 < argc) probe = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
        else if (std::strcmp(argv[i], "--noise-dir") == 0 && i + 1 < argc) noiseDir = argv[++i];
    }
    if (out.empty()) out = ".local/probe/" + probe + ".json";
    if (probe == "rng") {
        ProbeRng(out);
        return 0;
    }
    if (probe == "noise") {
        ProbeNoise(out, noiseDir);
        return 0;
    }
    if (probe == "df") {
        const std::filesystem::path noiseDirPath(noiseDir);
        const std::filesystem::path dfDir = noiseDirPath.parent_path() / "df";
        ProbeDf(out, dfDir, noiseDirPath);
        return 0;
    }
    if (probe == "noises") {
        const std::filesystem::path noiseDirPath(noiseDir);
        const std::filesystem::path dfDir = noiseDirPath.parent_path() / "df";
        ProbeNoises(out, dfDir, noiseDirPath);
        return 0;
    }
    std::fprintf(stderr, "unknown probe '%s'\n", probe.c_str());
    return 2;
}
