// CraftEngine - Java 26.3 density functions.
//
// Direct interpreter of the vanilla density function JSON
// (data/minecraft/worldgen/density_function). The Java oracle
// (tools/oracle/DfOracle.java) builds real 26.3 op objects and samples them via
// DensityFunction.compileSampler + SamplerContext.EMPTY_UNCACHED; this file is
// the C++ port. The differential harness compares the two at identical points.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "json.h"
#include "noise.h"

namespace mc {

struct DfInterp {
    explicit DfInterp(const std::filesystem::path& dfDir,
                      const std::filesystem::path& noiseDir);

    // routerKeys: the 8 overworld router entries (with "minecraft:" prefix).
    void run(const std::string& outPath, const std::vector<std::string>& routerKeys);

    std::map<std::string, Json> files;
    std::map<std::string, std::unique_ptr<Noise>> noises;
    PositionalRandomFactory* factory = nullptr;

private:
    const Json* file(const std::string& name); // dfDir/<name>.json, lazy
    const Json* noiseFile(const std::string& name); // noiseDir/<name>.json, lazy
    Noise* noiseFor(const std::string& id);
    void collectNoises(const Json& j, std::map<std::string, Json>& seen);
    float sampleRef(const Json& node, int x, int y, int z);
    float sampleNode(const Json& node, int x, int y, int z);

    std::filesystem::path dfDir_;
    std::filesystem::path noiseDir_;
    std::map<std::string, Json> noiseFiles_;
};

} // namespace mc
