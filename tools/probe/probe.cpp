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
#include <string>
#include <vector>

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
    std::string out = ".local/probe/rng.json";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--probe") == 0 && i + 1 < argc) probe = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
    }
    if (probe == "rng") {
        ProbeRng(out);
        return 0;
    }
    std::fprintf(stderr, "unknown probe '%s'\n", probe.c_str());
    return 2;
}
