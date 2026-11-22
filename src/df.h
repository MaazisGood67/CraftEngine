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
#include <set>
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

    // Debug: build the graph like run(), then dump every shared Noise instance at
    // one point in creation order. Used to attribute a density mismatch to a
    // specific noise instead of guessing (see tools/oracle/DfDebug.java).
    void dumpNoises(const std::string& outPath, const std::vector<std::string>& routerKeys,
                    double px, double py, double pz);

    std::map<std::string, Json> files;
    // Java creates a separate Noise for every occurrence of a noise node, all
    // from the same RandomSource(1337). Ownership is kept here; sampling uses
    // the instance assigned to the node during the build phase.
    std::vector<std::unique_ptr<Noise>> noises;
    std::map<const Json*, Noise*> noiseByNode;
    // One Noise per noise id, shared by all referencing nodes (RandomState.getOrCreateNoise).
    std::map<std::string, Noise*> noiseById_;
    std::vector<std::string> noiseOrder_; // noise ids in creation order
    RandomSource* rng = nullptr;

private:
    const Json* file(const std::string& name); // dfDir/<name>.json, lazy
    const Json* noiseFile(const std::string& name); // noiseDir/<name>.json, lazy
    Noise* createNoise(const std::string& id);
    Noise* noiseForNode(const Json& node);
    void buildRef(const std::string& id);
    void buildValue(const Json& node);
    void buildOp(const Json& j);
    void buildChild(const Json& j, const char* key);
    // Codec order for a spline node: coordinate first, then each point's value
    // (a number or a nested spline), which is when vanilla creates its noises.
    void buildSpline(const Json& spline);
    std::set<std::string> built_;
    float sampleRef(const Json& node, int x, int y, int z);
    // CubicSpline<Coordinate>.sample, see src/df.cpp for the bytecode citation.
    float sampleSpline(const Json& spline, int x, int y, int z);
    // Java child() defaults a missing node to ConstantFunction(0).
    float ref(const Json& node, const char* key, int x, int y, int z);
    float sampleNode(const Json& node, int x, int y, int z);

    std::filesystem::path dfDir_;
    std::filesystem::path noiseDir_;
    std::map<std::string, Json> noiseFiles_;
};

} // namespace mc
