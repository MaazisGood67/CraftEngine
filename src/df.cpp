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

// Mth.lerp(float delta, float start, float end): verified in Mth.txt.
inline float Lerp(float delta, float start, float end) {
    return start + delta * (end - start);
}

// Java Math.floorMod for the positive cell sizes used by interpolated/slice.
inline int FloorMod(int v, int m) {
    if (m <= 0) return 0;
    const int r = v % m;
    return r < 0 ? r + m : r;
}

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

Noise* DfInterp::createNoise(const std::string& id0) {
    std::string id = id0;
    if (id.rfind("minecraft:", 0) == 0) id = id.substr(10);
    // RandomState.getOrCreateNoise: noiseInstances.computeIfAbsent(key) - one Noise
    // per noise id, created on first use from the shared RandomSource and then
    // shared by every density function that references it.
    auto cached = noiseById_.find(id);
    if (cached != noiseById_.end()) return cached->second;
    std::fprintf(stderr, "NOISE %s\n", id.c_str());
    const Json* j = noiseFile(id);
    if (!j) {
        std::fprintf(stderr, "missing noise %s\n", id.c_str());
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
    if (!rng) return nullptr;
    noises.push_back(normal.create(*rng));
    noiseById_[id] = noises.back().get();
    noiseOrder_.push_back(id);
    return noises.back().get();
}

Noise* DfInterp::noiseForNode(const Json& node) {
    auto it = noiseByNode.find(&node);
    return it == noiseByNode.end() ? nullptr : it->second;
}

void DfInterp::buildRef(const std::string& id) {
    if (!built_.insert(id).second) return;
    const Json* j = file(id);
    if (j) buildOp(*j);
}

void DfInterp::buildValue(const Json& node) {
    if (node.type == Json::String && !node.str.empty()) {
        if (node.str.rfind("minecraft:", 0) == 0) {
            buildRef(node.str.substr(10));
        }
        return;
    }
    buildOp(node);
}

void DfInterp::buildChild(const Json& j, const char* key) {
    if (const Json* p = j.find(key)) buildValue(*p);
}

void DfInterp::buildOp(const Json& j) {
    const std::string name = TypeOf(j);
    if (name == "noise") {
        Noise* n = createNoise(Str(j.find("noise")));
        noiseByNode[&j] = n;
        // Java buildNoiseFunction creates the noise first, then compiles the
        // shift_x/shift_y/shift_z child functions in that order.
        buildChild(j, "shift_x");
        buildChild(j, "shift_y");
        buildChild(j, "shift_z");
        return;
    }
    if (name == "shift_a" || name == "shift_b") {
        noiseByNode[&j] = createNoise(Str(j.find("noise")));
        return;
    }
    if (name == "add" || name == "sub" || name == "mul" || name == "div" ||
        name == "min" || name == "max") {
        buildChild(j, "left");
        buildChild(j, "right");
        return;
    }
    if (name == "abs" || name == "square" || name == "cube" || name == "half_negative" ||
        name == "quarter_negative" || name == "negate" || name == "reciprocal" ||
        name == "squeeze" || name == "log" || name == "sign" || name == "cache" ||
        name == "clamp" || name == "interpolated" || name == "blend_density" ||
        name == "source") {
        buildChild(j, "input");
        return;
    }
    if (name == "lerp") {
        buildChild(j, "alpha");
        buildChild(j, "first");
        buildChild(j, "second");
        return;
    }
    if (name == "range_choice") {
        buildChild(j, "input");
        buildChild(j, "when_in_range");
        buildChild(j, "when_out_of_range");
        return;
    }
    if (name == "find_top_surface") {
        buildChild(j, "density");
        if (j.find("upper_bound")) buildChild(j, "upper_bound");
        else buildChild(j, "density");
        return;
    }
    if (name == "interval_select") {
        buildChild(j, "input");
        if (const Json* fs = j.find("functions")) {
            if (fs->type == Json::Array) for (const Json& f : fs->arr) buildValue(f);
        }
        return;
    }
    if (name == "old_blended_noise") {
        // BlendedNoise.compileSampler calls ctx.createRandom(NOISE_SEED); the oracle
        // hands back a fresh XoroshiroRandomSource(1337) for it, so the stack seeds
        // are fixed and can be built once per node.
        XoroshiroRandom blendedRandom(1337);
        blendedByNode[&j] = std::make_unique<BlendedNoise>(
                blendedRandom, D(j.find("xz_scale")), D(j.find("y_scale")),
                D(j.find("xz_factor")), D(j.find("y_factor")),
                D(j.find("smear_scale_multiplier")));
        return;
    }
    if (name == "slice") { buildChild(j, "input"); return; }
    if (name == "spline") {
        if (const Json* sp = j.find("spline")) buildSpline(*sp);
        return;
    }
}

void DfInterp::buildSpline(const Json& spline) {
    if (const Json* coord = spline.find("coordinate")) buildValue(*coord);
    if (const Json* points = spline.find("points")) {
        if (points->type == Json::Array) {
            for (const Json& p : points->arr) {
                const Json* v = p.find("value");
                if (!v) continue;
                if (v->type == Json::Number) continue;
                if (const Json* nested = v->find("points")) {
                    (void)nested;
                    buildSpline(*v);
                }
            }
        }
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

float DfInterp::ref(const Json& j, const char* key, int x, int y, int z) {
    const Json* el = j.find(key);
    return el ? sampleRef(*el, x, y, z) : 0.0f;
}

// CubicSpline<Coordinate>.sample, straight from CubicSpline$Multipoint.txt:
//   f = coordinate(input); i = findIntervalStart(locations, f);
//   i < 0     -> linearExtend(f, locations, values[0], derivatives, 0)
//   i == last -> linearExtend(f, locations, values[last], derivatives, last)
//   else      -> lerp(h, v0, v1) + h*(1-h)*lerp(h, p, q)
//     h  = (f - l0) / (l1 - l0)
//     p  = d0*(l1-l0) - (v1-v0)
//     q  = -d1*(l1-l0) + (v1-v0)
float DfInterp::sampleSpline(const Json& spline, int x, int y, int z) {
    const Json* coord = spline.find("coordinate");
    const Json* points = spline.find("points");
    if (!coord || !points || points->type != Json::Array || points->arr.empty()) return 0.0f;

    const std::vector<Json>& pts = points->arr;
    std::vector<float> locations, derivatives;
    locations.reserve(pts.size());
    derivatives.reserve(pts.size());
    for (const Json& p : pts) {
        locations.push_back(F(p.find("location")));
        derivatives.push_back(F(p.find("derivative")));
    }

    const float f = coord->type == Json::Number ? static_cast<float>(coord->num)
                                                : sampleRef(*coord, x, y, z);

    // Mth.binarySearch(0, n, i -> f < locations[i]) - 1  ==  last i with locations[i] <= f
    int idx = -1;
    for (int i = static_cast<int>(locations.size()) - 1; i >= 0; --i) {
        if (locations[i] <= f) { idx = i; break; }
    }
    const int last = static_cast<int>(locations.size()) - 1;

    auto valueAt = [&](int i) {
        const Json* v = pts[i].find("value");
        if (!v) return 0.0f;
        if (v->type == Json::Number) return static_cast<float>(v->num);
        return sampleSpline(*v, x, y, z);
    };

    // CubicSpline.linearExtend: derivatives[index] == 0 ? value
    //                          : value + derivatives[index]*(f - locations[index])
    auto linearExtend = [&](int i) {
        const float v = valueAt(i);
        return derivatives[i] == 0.0f ? v : v + derivatives[i] * (f - locations[i]);
    };

    if (idx < 0) return linearExtend(0);
    if (idx == last) return linearExtend(last);

    const float l0 = locations[idx];
    const float l1 = locations[idx + 1];
    const float h = (f - l0) / (l1 - l0);
    const float d0 = derivatives[idx];
    const float d1 = derivatives[idx + 1];
    const float v0 = valueAt(idx);
    const float v1 = valueAt(idx + 1);
    const float span = l1 - l0;
    const float p = d0 * span - (v1 - v0);
    const float q = -d1 * span + (v1 - v0);
    return Lerp(h, v0, v1) + h * (1.0f - h) * Lerp(h, p, q);
}

float DfInterp::sampleNode(const Json& j, int x, int y, int z) {
    static const bool trace = [] {
        const char* e = std::getenv("MC_DF_TRACE");
        return e && *e && *e != '0';
    }();
    static int traceBudget = 4096;
    const std::string type = TypeOf(j);
    const double xd = static_cast<double>(x);
    const double yd = static_cast<double>(y);
    const double zd = static_cast<double>(z);
    float v = 0.0f;

    if (type == "add") {
        v = ref(j, "left", x, y, z) + ref(j, "right", x, y, z);
    } else if (type == "sub") {
        v = ref(j, "left", x, y, z) - ref(j, "right", x, y, z);
    } else if (type == "mul") {
        v = ref(j, "left", x, y, z) * ref(j, "right", x, y, z);
    } else if (type == "div") {
        v = ref(j, "left", x, y, z) / ref(j, "right", x, y, z);
    } else if (type == "min") {
        const float l = ref(j, "left", x, y, z);
        const float r = ref(j, "right", x, y, z);
        v = l < r ? l : r;
    } else if (type == "max") {
        const float l = ref(j, "left", x, y, z);
        const float r = ref(j, "right", x, y, z);
        v = l > r ? l : r;
    } else if (type == "abs") {
        v = std::fabs(ref(j, "input", x, y, z));
    } else if (type == "square") {
        const float a = ref(j, "input", x, y, z);
        v = a * a;
    } else if (type == "cube") {
        const float a = ref(j, "input", x, y, z);
        v = a * a * a;
    } else if (type == "half_negative" || type == "quarter_negative") {
        // 26.3 maps these to UnaryFunction$LeakyReLUSampler with negativeFactor 0.5f / 0.25f:
        //   apply(x) = x > 0 ? x : x * negativeFactor   (UnaryFunction$LeakyReLUSampler.txt)
        // i.e. the NEGATIVE side is damped, not the positive one.
        const float a = ref(j, "input", x, y, z);
        const float factor = type == "half_negative" ? 0.5f : 0.25f;
        v = a > 0.0f ? a : a * factor;
    } else if (type == "negate") {
        v = -ref(j, "input", x, y, z);
    } else if (type == "reciprocal") {
        v = 1.0f / ref(j, "input", x, y, z);
    } else if (type == "squeeze") {
        // 26.3 UnaryFunction$SqueezeSampler.apply(float):
        //   f = Mth.clamp(x, -1, 1); return f/2 - Mth.cube(f)/24
        // (not the pre-1.21 c*(3-c^2)/2 form).
        const float a = ref(j, "input", x, y, z);
        const float c = a < -1.0f ? -1.0f : (a > 1.0f ? 1.0f : a);
        v = c / 2.0f - (c * c * c) / 24.0f;
    } else if (type == "log") {
        v = std::log(ref(j, "input", x, y, z));
    } else if (type == "sign") {
        const float a = ref(j, "input", x, y, z);
        v = a == 0.0f ? 0.0f : (a > 0.0f ? 1.0f : -1.0f);
    } else if (type == "clamp") {
        const float a = ref(j, "input", x, y, z);
        const float lo = F(j.find("min"));
        const float hi = F(j.find("max"));
        v = a < lo ? lo : (a > hi ? hi : a);
    } else if (type == "lerp") {
        // LerpFunction$Sampler.sampleValue short-circuits: alpha == 0 returns `first`,
        // alpha == 1 returns `second`, otherwise Mth.lerp(alpha, first, second).
        // The shortcut matters: `first + 1.0f*(second-first)` is not bit-identical
        // to `second` in float arithmetic.
        const float alpha = ref(j, "alpha", x, y, z);
        if (alpha == 0.0f) {
            v = ref(j, "first", x, y, z);
        } else if (alpha == 1.0f) {
            v = ref(j, "second", x, y, z);
        } else {
            const float first = ref(j, "first", x, y, z);
            const float second = ref(j, "second", x, y, z);
            v = Lerp(alpha, first, second);
        }
    } else if (type == "range_choice") {
        const float in = ref(j, "input", x, y, z);
        const float lo = F(j.find("min_inclusive"));
        const float hi = F(j.find("max_exclusive"));
        v = (in >= lo && in < hi)
                ? ref(j, "when_in_range", x, y, z)
                : ref(j, "when_out_of_range", x, y, z);
    } else if (type == "interpolated") {
        // InterpolatedFunction$Sampler.sampleValue (InterpolatedFunction$Sampler.txt):
        // on a cell corner it delegates; otherwise it samples the 8 corners of the
        // enclosing cell and combines them with Mth.lerp3(fx, fy, fz, c000..c111),
        // whose exact order is z-outermost, x-innermost (see Lerp3 below).
        const int cxz = I(j.find("cell_size_xz"));
        const int cy = I(j.find("cell_size_y"));
        const int fx = FloorMod(x, cxz);
        const int fy = FloorMod(y, cy);
        const int fz = FloorMod(z, cxz);
        if (fx == 0 && fy == 0 && fz == 0) {
            v = ref(j, "input", x, y, z);
        } else {
            const int x0 = x - fx;
            const int y0 = y - fy;
            const int z0 = z - fz;
            const float c000 = ref(j, "input", x0, y0, z0);
            const float c100 = ref(j, "input", x0 + cxz, y0, z0);
            const float c010 = ref(j, "input", x0, y0 + cy, z0);
            const float c110 = ref(j, "input", x0 + cxz, y0 + cy, z0);
            const float c001 = ref(j, "input", x0, y0, z0 + cxz);
            const float c101 = ref(j, "input", x0 + cxz, y0, z0 + cxz);
            const float c011 = ref(j, "input", x0, y0 + cy, z0 + cxz);
            const float c111 = ref(j, "input", x0 + cxz, y0 + cy, z0 + cxz);
            // Java stores 1.0f/(float)cellSize and multiplies by the block offset.
            const float dx = static_cast<float>(fx) * (1.0f / static_cast<float>(cxz));
            const float dy = static_cast<float>(fy) * (1.0f / static_cast<float>(cy));
            const float dz = static_cast<float>(fz) * (1.0f / static_cast<float>(cxz));
            // Mth.lerp2(fx, fy, v000, v100, v010, v110) = lerp(fy, lerp(fx,v000,v100), lerp(fx,v010,v110))
            auto lerp2 = [](float dX, float dY, float a, float b, float c, float d) {
                return Lerp(dY, Lerp(dX, a, b), Lerp(dX, c, d));
            };
            const float z0v = lerp2(dx, dy, c000, c100, c010, c110);
            const float z1v = lerp2(dx, dy, c001, c101, c011, c111);
            v = Lerp(dz, z0v, z1v);
        }
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
                if (ref(j, "density", x, i, z) > 0.0f) {
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
        Noise* n = noiseForNode(j);
        if (!n) return 0.0f;
        // NoiseFunction.codec defaults: xz_scale 0.25, y_scale 0.0.
        const double xzScale = Has(j, "xz_scale") ? D(j.find("xz_scale")) : 0.25;
        const double yScale = Has(j, "y_scale") ? D(j.find("y_scale")) : 0.0;
        // NoiseFunction.compileSampler installs ShiftedXzSampler (or ShiftedXyzSampler
        // when shift_y is present). NoiseFunction$ShiftedXzSampler.sampleValue:
        //   d = x*xzScale + (double)shiftX.sampleValue(...)
        //   e = y*yScale
        //   f = z*xzScale + (double)shiftZ.sampleValue(...)
        //   return noise.get(d, e, f)
        double d = xd * xzScale;
        double e = yd * yScale;
        double f2 = zd * xzScale;
        if (Has(j, "shift_x")) d += static_cast<double>(ref(j, "shift_x", x, y, z));
        if (Has(j, "shift_z")) f2 += static_cast<double>(ref(j, "shift_z", x, y, z));
        if (Has(j, "shift_y")) e += static_cast<double>(ref(j, "shift_y", x, y, z));
        v = n->get(d, e, f2);
        if (trace && traceBudget > 0) {
            --traceBudget;
            std::fprintf(stderr, "TRACE noise %s at (%d,%d,%d) d=%.17g e=%.17g f=%.17g -> %.9g\n",
                         Str(j.find("noise")).c_str(), x, y, z, d, e, f2, static_cast<double>(v));
        }
    } else if (type == "shift_a" || type == "shift_b") {
        Noise* n = noiseForNode(j);
        if (!n) return 0.0f;
        // ShiftNoiseFunction$ShiftA.compileSampler -> NoiseFunction$Sampler(noise, 0.25, 0.0)
        //   then ConstMulSampler(4.0f): noise.get(x*0.25, 0.0, z*0.25) * 4
        // ShiftNoiseFunction$ShiftB$1.sampleValue (bytecode):
        //   noise.get(z*0.25, x*0.25, 0.0) * 4.0f  -- the axes are swapped.
        if (type == "shift_a") {
            v = static_cast<float>(n->get(xd * 0.25, 0.0, zd * 0.25) * 4.0);
        } else {
            v = static_cast<float>(n->get(zd * 0.25, xd * 0.25, 0.0) * 4.0);
        }
    } else if (type == "cache") {
        v = ref(j, "input", x, y, z);
    } else if (type == "constant") {
        v = F(j.find("value"));
    } else if (type == "beardifier") {
        // SimpleDensityFunction.BEARDIFIER with an empty context resolves to 0.
        v = 0.0f;
    } else if (type == "blend_alpha") {
        // SimpleDensityFunction.BLEND_ALPHA default (Blender.ALPHA_KEY).
        v = 1.0f;
    } else if (type == "blend_offset") {
        // SimpleDensityFunction.BLEND_OFFSET default (Blender.OFFSET_KEY).
        v = 0.0f;
    } else if (type == "blend_density") {
        // BlendDensityFunction: with no blend context, returns input unchanged.
        v = ref(j, "input", x, y, z);
    } else if (type == "distance_to_point") {
        v = 0.0f;
    } else if (type == "old_blended_noise") {
        // Built in buildOp (the stack seeds come from a fresh RandomSource(1337)).
        auto it = blendedByNode.find(&j);
        v = (it != blendedByNode.end() && it->second)
                ? it->second->get(xd, yd, zd)
                : 0.0f;
        if (trace && traceBudget > 0) {
            --traceBudget;
            std::fprintf(stderr, "TRACE blended at (%d,%d,%d) -> %.9g\n", x, y, z,
                         static_cast<double>(v));
        }
    } else if (type == "interval_select") {
        // Verified from IntervalSelectFunction$Sampler.selectSamplerIndex bytecode:
        // choose the first function whose threshold is strictly greater than the
        // input; if none is greater, choose the last function.
        const float in = ref(j, "input", x, y, z);
        const Json* th = j.find("thresholds");
        const Json* fns = j.find("functions");
        size_t idx = 0;
        if (th && th->type == Json::Array) {
            while (idx < th->arr.size() && !(in < static_cast<float>(th->arr[idx].num))) ++idx;
        }
        if (fns && fns->type == Json::Array && !fns->arr.empty()) {
            if (idx >= fns->arr.size()) idx = fns->arr.size() - 1;
            v = sampleRef(fns->arr[idx], x, y, z);
        }
    } else if (type == "slice") {
        v = 0.0f;
    } else if (type == "spline") {
        // SplineFunction wraps CubicSpline<Coordinate>; the JSON key is "spline".
        const Json* sp = j.find("spline");
        v = sp ? sampleSpline(*sp, x, y, z) : 0.0f;
    } else {
        std::fprintf(stderr, "df op unsupported: %s keys(at)%s\n", type.c_str(),
                     (j.find("type") ? "" : " (no type)"));
        v = 0.0f;
    }
    return v;
}

void DfInterp::run(const std::string& outPath, const std::vector<std::string>& routerKeys) {
    XoroshiroRandom random(1337);
    this->rng = &random;

    // Build the whole router graph up front, in the same order as DfOracle's
    // ROUTER_KEYS loop. Every Noise instance is created from this one RandomSource
    // in DFS preorder, which is what makes the seeds match the oracle.
    for (const std::string& key : routerKeys) buildRef(key);

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
    static const int ZS[24] = {12, 300, -40, 55, 88, -12, 123, 224, -74, 9, 190, 42,
                               66, -3, 12, 300, -40, 55, 88, -12, 123, 224, -74, 9};

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

void DfInterp::dumpNoises(const std::string& outPath, const std::vector<std::string>& routerKeys,
                         double px, double py, double pz) {
    XoroshiroRandom random(1337);
    this->rng = &random;
    for (const std::string& key : routerKeys) buildRef(key);

    FILE* f = fopen(outPath.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
        return;
    }
    std::fprintf(f, "{\n  \"version\":\"26.3\",\n  \"noise_order\":[\n");
    for (size_t i = 0; i < noiseOrder_.size(); ++i) {
        Noise* n = noiseById_[noiseOrder_[i]];
        const float v = n ? n->get(px, py, pz) : 0.0f;
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        std::fprintf(f, "    {\"id\":\"%s\",\"v\":\"%u\"}%s\n", noiseOrder_[i].c_str(),
                     static_cast<unsigned>(bits), i + 1 == noiseOrder_.size() ? "" : ",");
    }
    std::fprintf(f, "  ]\n}\n");
    fclose(f);
    std::printf("wrote %s (%d noises)\n", outPath.c_str(), static_cast<int>(noiseOrder_.size()));
}

float DfInterp::sampleById(const std::string& id, const std::vector<std::string>& routerKeys,
                           int x, int y, int z) {
    XoroshiroRandom random(1337);
    this->rng = &random;
    for (const std::string& key : routerKeys) buildRef(key);
    buildRef(id);
    const Json* j = file(id);
    return j ? sampleNode(*j, x, y, z) : 0.0f;
}

float DfInterp::sampleJsonText(const std::string& jsonText,
                               const std::vector<std::string>& routerKeys, int x, int y, int z) {
    XoroshiroRandom random(1337);
    this->rng = &random;
    for (const std::string& key : routerKeys) buildRef(key);

    Json extra;
    if (!ParseJson(jsonText, extra)) return 0.0f;
    buildOp(extra);
    return sampleNode(extra, x, y, z);
}

} // namespace mc
