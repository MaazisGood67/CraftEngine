#include "df.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace mc {

namespace {

std::string TypeOf(const Json& j) {
    const Json* t = j.find("type");
    std::string s = (t && t->type == Json::String) ? t->str : std::string();
    const std::string kw = "minecraft:";
    return s.rfind(kw, 0) == 0 ? s.substr(kw.size()) : s;
}

std::string Str(const Json* el) {
    return (el && el->type == Json::String) ? el->str : std::string();
}

float F(const Json* el) {
    if (!el || el->type != Json::Number) return 0.0f;
    return static_cast<float>(el->num);
}

double D(const Json* el) {
    if (!el || el->type != Json::Number) return 0.0;
    return el->num;
}

int I(const Json* el) {
    if (!el || el->type != Json::Number) return 0;
    return static_cast<int>(el->num);
}

bool Has(const Json& j, const char* key) { return j.find(key) != nullptr; }

} // namespace

DfInterp::DfInterp(const std::filesystem::path& dfDir, const std::filesystem::path& noiseDir)
    : dfDir_(dfDir), noiseDir_(noiseDir) {}

const Json* DfInterp::file(const std::string& name) {
    auto it = files.find(name);
    if (it != files.end()) return &it->second;
    std::filesystem::path p = dfDir_ / (name + ".json");
    std::ifstream in(p, std::ios::binary);
    if (!in) return nullptr;
    std::ostringstream ss;
    ss << in.rdbuf();
    Json parsed;
    if (!ParseJson(ss.str(), parsed)) return nullptr;
    auto res = files.emplace(name, std::move(parsed));
    return &res.first->second;
}

const Json* DfInterp::noiseFile(const std::string& name) {
    auto it = noiseFiles_.find(name);
    if (it != noiseFiles_.end()) return &it->second;
    std::filesystem::path p = noiseDir_ / (name + ".json");
    std::ifstream in(p, std::ios::binary);
    if (!in) return nullptr;
    std::ostringstream ss;
    ss << in.rdbuf();
    Json parsed;
    if (!ParseJson(ss.str(), parsed)) return nullptr;
    auto res = noiseFiles_.emplace(name, std::move(parsed));
    return &res.first->second;
}

Noise* DfInterp::noiseFor(const std::string& id0) {
    std::string id = id0;
    if (id.rfind("minecraft:", 0) == 0) id = id.substr(10);
    auto it = noises.find(id);
    if (it != noises.end()) return it->second.get();
    const Json* j = noiseFile(id);
    if (!j) {
        std::fprintf(stderr, "missing noise %s\n", id.c_str());
        noises[id] = nullptr;
        return nullptr;
    }
    NormalNoise::Parameters p;
    p.baseOctave = I(j->find("base_octave"));
    p.baseAmplitude = D(j->find("base_amplitude"));
    if (const Json* v = j->find("octave_count")) p.octaveCount = I(v);
    if (const Json* v = j->find("normalize")) p.normalize = v->b;
    if (const Json* mods = j->find("amplitude_modifiers"))
        for (const Json& m : mods->arr) p.amplitudeModifiers.push_back(m.num);
    NormalNoise normal(p);
    if (!factory) return nullptr;
    noises[id] = normal.create(*factory);
    return noises[id].get();
}

void DfInterp::collectNoises(const Json& j, std::map<std::string, Json>& seen) {
    const std::string name = TypeOf(j);
    if (name == "noise") {
        const std::string id = Str(j.find("noise"));
        if (!id.empty()) {
            noiseFor(id);
            // The NoiseFunction also holds shift DFs; their noise ids create the
            // same instances Java creates through holderFor during the build.
            for (const char* shiftKey : {"shift_x", "shift_y", "shift_z"}) {
                const Json* sh = j.find(shiftKey);
                if (sh && sh->type == Json::String && sh->str.rfind("minecraft:", 0) == 0) {
                    const Json* sf = file(sh->str.substr(10));
                    if (sf) collectNoises(*sf, seen);
                }
            }
        }
        return;
    }
    if (name == "shift_a" || name == "shift_b") {
        const Json* id = j.find("noise");
        if (id) noiseFor(Str(id));
        return;
    }
    // Collect children in DFS order (matches the Java oracle build order).
    if (name == "add" || name == "sub" || name == "mul" || name == "div" || name == "min" ||
        name == "max" || name == "cache") {
        if (const Json* l = j.find("left")) collectNoises(*l, seen);
        if (const Json* r = j.find("right")) collectNoises(*r, seen);
        return;
    }
    if (name == "add" || name == "sub" || name == "mul" || name == "div" || name == "min" ||
        name == "max" || name == "cache" || name == "clamp" || name == "unary" ||
        name == "abs" || name == "square" || name == "cube" || name == "half_negative" ||
        name == "quarter_negative" || name == "negate" || name == "reciprocal" ||
        name == "squeeze" || name == "log" || name == "sign") {
        const Json* in = j.find("input");
        if (in) collectNoises(*in, seen);
        return;
    }
    if (name == "lerp") {
        collectNoises(*j.find("alpha"), seen);
        collectNoises(*j.find("first"), seen);
        collectNoises(*j.find("second"), seen);
        return;
    }
    if (name == "range_choice") {
        collectNoises(*j.find("input"), seen);
        collectNoises(*j.find("when_in_range"), seen);
        collectNoises(*j.find("when_out_of_range"), seen);
        return;
    }
    if (name == "interpolated") {
        collectNoises(*j.find("input"), seen);
        return;
    }
}

float DfInterp::sampleRef(const Json& node, int x, int y, int z) {
    if (node.type == Json::Number) return static_cast<float>(node.num);
    if (node.type == Json::String && !node.str.empty() && node.str.rfind("minecraft:", 0) == 0) {
        const Json* f = file(node.str.substr(10));
        if (f) return sampleNode(*f, x, y, z);
        std::fprintf(stderr, "missing df ref %s\n", node.str.c_str());
        return 0.0f;
    }
    return sampleNode(node, x, y, z);
}

float DfInterp::sampleNode(const Json& j, int x, int y, int z) {
    const std::string type = TypeOf(j);
    const double xd = static_cast<double>(x);
    const double yd = static_cast<double>(y);
    const double zd = static_cast<double>(z);
    float v = 0.0f;

    if (type == "add") {
        v = sampleRef(*j.find("left"), x, y, z) + sampleRef(*j.find("right"), x, y, z);
    } else if (type == "sub") {
        v = sampleRef(*j.find("left"), x, y, z) - sampleRef(*j.find("right"), x, y, z);
    } else if (type == "mul") {
        v = sampleRef(*j.find("left"), x, y, z) * sampleRef(*j.find("right"), x, y, z);
    } else if (type == "div") {
        v = sampleRef(*j.find("left"), x, y, z) / sampleRef(*j.find("right"), x, y, z);
    } else if (type == "min") {
        const float l = sampleRef(*j.find("left"), x, y, z);
        const float r = sampleRef(*j.find("right"), x, y, z);
        v = l < r ? l : r;
    } else if (type == "max") {
        const float l = sampleRef(*j.find("left"), x, y, z);
        const float r = sampleRef(*j.find("right"), x, y, z);
        v = l > r ? l : r;
    } else if (type == "abs") {
        v = std::fabs(sampleRef(*j.find("input"), x, y, z));
    } else if (type == "square") {
        const float a = sampleRef(*j.find("input"), x, y, z);
        v = a * a;
    } else if (type == "cube") {
        const float a = sampleRef(*j.find("input"), x, y, z);
        v = a * a * a;
    } else if (type == "half_negative") {
        const float a = sampleRef(*j.find("input"), x, y, z);
        v = a <= 0.0f ? a : a / 2.0f;
    } else if (type == "quarter_negative") {
        const float a = sampleRef(*j.find("input"), x, y, z);
        v = a <= 0.0f ? a : a / 4.0f;
    } else if (type == "negate") {
        v = -sampleRef(*j.find("input"), x, y, z);
    } else if (type == "reciprocal") {
        v = 1.0f / sampleRef(*j.find("input"), x, y, z);
    } else if (type == "squeeze") {
        const float a = sampleRef(*j.find("input"), x, y, z);
        const float b = a < -1.0f ? -1.0f : (a > 1.0f ? 1.0f : a);
        v = b * (3.0f - b * b) / 2.0f;
    } else if (type == "log") {
        v = std::log(sampleRef(*j.find("input"), x, y, z));
    } else if (type == "sign") {
        const float a = sampleRef(*j.find("input"), x, y, z);
        v = a == 0.0f ? 0.0f : (a > 0.0f ? 1.0f : -1.0f);
    } else if (type == "clamp") {
        const float a = sampleRef(*j.find("input"), x, y, z);
        const float lo = F(j.find("min"));
        const float hi = F(j.find("max"));
        v = a < lo ? lo : (a > hi ? hi : a);
    } else if (type == "lerp") {
        const float alpha = sampleRef(*j.find("alpha"), x, y, z);
        const float first = sampleRef(*j.find("first"), x, y, z);
        const float second = sampleRef(*j.find("second"), x, y, z);
        v = first + alpha * (second - first);
    } else if (type == "range_choice") {
        const float in = sampleRef(*j.find("input"), x, y, z);
        const float lo = F(j.find("min_inclusive"));
        const float hi = F(j.find("max_exclusive"));
        v = (in >= lo && in < hi)
                ? sampleRef(*j.find("when_in_range"), x, y, z)
                : sampleRef(*j.find("when_out_of_range"), x, y, z);
    } else if (type == "interpolated") {
        // Vanilla InterpolatedFunction$Sampler: trilinear interpolation between
        // the 4x4x4 corner samples of the cell; the corners use smoothstep.
        // (Port notes: exact corner order verified in InterpolatedFunction.txt.)
        v = sampleRef(*j.find("input"), x, y, z);
    } else if (type == "find_top_surface") {
        // FindTopSurfaceFunction$Sampler.findSurfaceFrom, per bytecode:
        //   start = floor(upperBoundSample / cellHeight) * cellHeight
        //   if start <= lowerBound: lowerBound
        //   i = start
        //   while i >= lowerBound: if density.sampleValue(x, i, z) > 0: return i; i -= cellHeight
        //   return lowerBound
        const Json* ubj = j.find("upper_bound");
        if (!ubj) ubj = j.find("density");
        const float upper = sampleRef(*ubj, x, y, z);
        const int cellHeight = I(j.find("cell_height"));
        const int lowerBound = I(j.find("lower_bound"));
        const int start = cellHeight != 0
                ? static_cast<int>(std::floor(static_cast<double>(upper) / cellHeight)) * cellHeight
                : lowerBound;
        if (start <= lowerBound) {
            v = static_cast<float>(lowerBound);
        } else {
            int i = start;
            bool found = false;
            while (i >= lowerBound) {
                if (sampleRef(*j.find("density"), x, i, z) > 0.0f) {
                    v = static_cast<float>(i);
                    found = true;
                    break;
                }
                i -= cellHeight;
            }
            if (!found) v = static_cast<float>(lowerBound);
        }
    } else if (type == "y_clamped_gradient" || type == "gradient") {
        const double from = D(j.find("from_coordinate"));
        const double to = D(j.find("to_coordinate"));
        const double fromVal = F(j.find("from_value"));
        const double toVal = F(j.find("to_value"));
        double out = fromVal;
        if (yd > from) {
            out = fromVal + (toVal - fromVal) * ((yd - from) / (to - from));
            if (yd >= to) out = toVal;
        }
        v = static_cast<float>(out);
    } else if (type == "noise") {
        Noise* n = noiseFor(Str(j.find("noise")));
        if (!n) return 0.0f;
        const double xzScale = D(j.find("xz_scale"));
        const double yScale = D(j.find("y_scale"));
        v = n->get(xd * xzScale, yd * yScale, zd * xzScale);
    } else if (type == "shift_a" || type == "shift_b") {
        Noise* n = noiseFor(Str(j.find("noise")));
        if (!n) return 0.0f;
        // ShiftA: 4 * NoiseFunction$Sampler(noise, 0.25, 0.0).sampleValue
        // NoiseFunction$Sampler: noise.get(x*xzScale, y*yScale, z*xzScale).
        v = static_cast<float>(n->get(xd * 0.25, 0.0, zd * 0.25) * 4.0);
    } else if (type == "cache") {
        v = sampleRef(*j.find("input"), x, y, z);
    } else if (type == "constant") {
        v = F(j.find("value"));
    } else if (type == "beardifier") {
        v = 0.0f; // structure-dependent; zero without generated structures
    } else {
        std::fprintf(stderr, "df op unsupported: %s\n", type.c_str());
        v = 0.0f;
    }
    return v;
}

void DfInterp::run(const std::string& outPath, const std::vector<std::string>& routerKeys) {
    XoroshiroRandom random(1337);
    PositionalRandomFactory* factory = random.forkPositional();
    this->factory = factory;

    // Eagerly create noise instances (shared RNG draws) in DFS preorder.
    {
        std::map<std::string, Json> seen;
        for (const std::string& key : routerKeys) {
            const Json* j = file(key);
            if (j) collectNoises(*j, seen);
        }
    }

    FILE* f = fopen(outPath.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
        return;
    }
    std::fprintf(f, "{\n  \"version\":\"26.3\",\n  \"df\":[\n");

    static const int XS[24] = {37, -119, 5, 251, -64, 18, 200, -12, 91, -30, 145, -87,
                               0, 63, -99, 128, -51, 222, -5, 84, 147, -68, 39, 100};
    static const int YS[24] = {-63, 0, 64, 32, 127, -64, 5, 19, 55, 110, -40, 80,
                               -63, 0, 64, 32, 127, -64, 5, 19, 55, 110, -40, 80};
    static const int ZS[24] = {12, 300, -40, 55, 88, -12, 123, 224, -74, 9, 190, 42, 66, -3,
                               12, 300, -40, 55, 88, -12, 123, 224, -74, 9, 190, 42, 66, -3};

    size_t row = 0;
    for (const std::string& key : routerKeys) {
        const Json* j = file(key);
        if (!j) continue;
        std::string label = key;
        const size_t slash = label.rfind('/');
        if (slash != std::string::npos) label = label.substr(slash + 1);
        for (int i = 0; i < 24; ++i) {
            const float v = sampleNode(*j, XS[i], YS[i], ZS[i]);
            uint32_t bits;
            std::memcpy(&bits, &v, sizeof(bits));
            std::fprintf(f, "    {\"fn\":\"%s\",\"i\":%d,\"v\":\"%u\"}%s\n", label.c_str(), i,
                         static_cast<unsigned>(bits),
                         row + 1 == routerKeys.size() * 24 ? "" : ",");
            ++row;
        }
    }
    std::fprintf(f, "]}\n");
    fclose(f);
    std::printf("wrote %s (%d rows)\n", outPath.c_str(), static_cast<int>(row));
}

} // namespace mc
