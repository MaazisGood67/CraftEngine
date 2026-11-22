import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;




/**
 * CraftEngine Java oracle: the real 26.3 density functions.
 *
 * Builds real Java DensityFunction objects using the vanilla op classes from
 * the jar (BinaryFunction, UnaryFunction, NoiseFunction, GradientFunction,
 * ConstantFunction, ShiftNoiseFunction$ShiftA/ShiftB, FindTopSurfaceFunction,
 * ...) via reflection, samples them through DensityFunction.compileSampler +
 * SamplerContext.EMPTY_UNCACHED + DensitySampler.sampleValue, and writes raw
 * float bits. The C++ probe (--probe df) interprets the same JSON files with
 * the same sampled points; tools/parity.ps1 diffs the outputs.
 *
 * This oracle uses java.* only: javac 21 cannot read the Java 25 class files
 * in the 26.3 jar, so all Mojang classes are referenced via reflection.
 */
public final class DfOracle {
    private static final Map<String, MiniJson.Json> FILES = new LinkedHashMap<>();
    private static final Map<String, Object> FNS = new LinkedHashMap<>();
    // Identity map: two NormalNoise objects with identical parameters are distinct
    // instances and must not collapse (equals-based maps did exactly that).
    private static final Map<Object, Object> SYNTH_BY_NOISE = new java.util.IdentityHashMap<>();
    private static final Map<String, Object> NORMAL_BY_ID = new LinkedHashMap<>();
    private static final List<String> ROUTER_KEYS = List.of(
            "chunk_surface_level", "continents", "depth", "erosion", "final_density",
            "ridges", "temperature", "vegetation");
    private static final int[] XS = {37, -119, 5, 251, -64, 18, 200, -12, 91, -30, 145, -87, 0, 63, -99, 128, -51, 222, -5, 84, 147, -68, 39, 100};
    private static final int[] YS = {-63, 0, 64, 32, 127, -64, 5, 19, 55, 110, -40, 80, -63, 0, 64, 32, 127, -64, 5, 19, 55, 110, -40, 80};
    private static final int[] ZS = {12, 300, -40, 55, 88, -12, 123, 224, -74, 9, 190, 42, 66, -3, 12, 300, -40, 55, 88, -12, 123, 224, -74, 9, 190, 42, 66, -3};

    private static final Object CTX;

    static {
        try {
            Class<?> ctxClass = Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction$CompileContext");
            java.lang.reflect.InvocationHandler handler = (proxy, method, args) -> {
                String name = method.getName();
                if ("createNoiseSampler".equals(name)) {
                    if (args[0] == null) return null;
                    return SYNTH_BY_NOISE.get(normalNoiseOf(args[0]));
                }
                if ("createRandom".equals(name) || "createEndIslandRandom".equals(name)) {
                    try { return newRandom(1337L); }
                    catch (Exception e) { throw new RuntimeException(e); }
                }
                return null;
            };
            CTX = java.lang.reflect.Proxy.newProxyInstance(ctxClass.getClassLoader(),
                    new Class<?>[]{ctxClass}, handler);
        } catch (Exception e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    private static Object random; // PositionalRandomFactory

    /** Bootstrap the game classes and the shared 1337 RandomSource (idempotent). */
    public static void init() throws Exception {
        try {
            Class.forName("net.minecraft.SharedConstants").getMethod("tryDetectVersion").invoke(null);
            Class.forName("net.minecraft.server.Bootstrap").getMethod("bootStrap").invoke(null);
        } catch (Throwable t) {
            System.err.println("bootstrap: " + t);
        }
        if (random == null) random = newRandom(1337L);
    }

    /** Debug hook (DfDebug): build the whole router graph so RNG order matches. */
    static void buildWholeRouter() throws Exception {
        for (String key : ROUTER_KEYS) buildRef("overworld/" + key);
    }

    /** Debug hook (DfDebug): build+sample an arbitrary DF JSON snippet. */
    static Object debugBuildValue(MiniJson.Json el) throws Exception { return buildValue(el); }

    static Object debugBuildRef(String id) throws Exception { return buildRef(id); }

    static float debugSample(Object fn, int x, int y, int z) throws Exception { return sample(fn, x, y, z); }

    /** Debug hook (DfDebug): sample every shared Noise in creation order. */
    static void debugDumpNoises(double px, double py, double pz) throws Exception {
        Method get = Class.forName("net.minecraft.world.level.levelgen.synth.Noise")
                .getMethod("get", double.class, double.class, double.class);
        StringBuilder sb = new StringBuilder("{\n  \"version\":\"26.3\",\n  \"noise_order\":[\n");
        int i = 0;
        for (Map.Entry<String, Object> e : NORMAL_BY_ID.entrySet()) {
            Object synth = SYNTH_BY_NOISE.get(e.getValue());
            float v = synth == null ? 0.0f : (Float) get.invoke(synth, px, py, pz);
            if (i > 0) sb.append(",\n");
            sb.append("    {\"id\":\"").append(e.getKey()).append("\",\"v\":\"")
              .append(Float.floatToRawIntBits(v) & 0xFFFFFFFFL).append("\"}");
            i++;
        }
        sb.append("\n  ]\n}\n");
        Files.writeString(Path.of(".local/oracle/noises.json"), sb.toString(), StandardCharsets.UTF_8);
        System.out.println("wrote .local/oracle/noises.json (" + i + " noises)");
    }

    public static void main(String[] args) throws Exception {
        init();
        String outPath = args.length > 0 ? args[0] : "df.json";

        List<String> entries = new ArrayList<>();
        for (String key : ROUTER_KEYS) {
            Object fn = buildRef("overworld/" + key);
            for (int i = 0; i < XS.length; i++) {
                entries.add("{\"fn\":\"" + key + "\",\"i\":" + i + ",\"v\":\"" + floatBits(sample(fn, XS[i], YS[i], ZS[i])) + "\"}");
            }
        }

        Files.writeString(Path.of(outPath),
                "{\"version\":\"26.3\",\"df\":[" + String.join(",", entries) + "]}\n",
                StandardCharsets.UTF_8);
        System.out.println("wrote " + outPath + " (" + entries.size() + " entries)");
    }

    // ------------------------------------------------------------------ plumbing
    private static String floatBits(float f) {
        return Integer.toUnsignedString(Float.floatToRawIntBits(f));
    }

    private static Object newRandom(long seed) throws Exception {
        return Class.forName("net.minecraft.world.level.levelgen.XoroshiroRandomSource")
                .getConstructor(long.class).newInstance(seed);
    }

    private static Object normalNoiseOf(Object holder) throws Exception {
        return holder.getClass().getMethod("value").invoke(holder);
    }

    private static Object synthFor(Object normalNoise) throws Exception {
        Object cached = SYNTH_BY_NOISE.get(normalNoise);
        if (cached != null) return cached;
        Object synth = normalNoise.getClass()
                .getMethod("create", Class.forName("net.minecraft.util.RandomSource"))
                .invoke(normalNoise, random);
        SYNTH_BY_NOISE.put(normalNoise, synth);
        return synth;
    }

    private static Object holderFor(String id) throws Exception {
        Object normalNoise = parseNormalNoise(id);
        Object synth = synthFor(normalNoise); // creates + registers the Noise
        return Class.forName("net.minecraft.core.Holder").getMethod("direct", Object.class).invoke(null, normalNoise);
    }

    private static float sample(Object fn, int x, int y, int z) throws Exception {
        // Use the real DensityFunctionCompiler: it deduplicates caches (as the
        // game does) before samplers are usable.
        Class<?> compilerC = Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunctionCompiler");
        Object compiler = compilerC.getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction$CompileContext")).newInstance(CTX);
        Object sampler = compilerC.getMethod("getSampler", Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                .invoke(compiler, fn);
        Object ctxEmpty = Class.forName("net.minecraft.world.level.levelgen.densityfunction.SamplerContext")
                .getField("EMPTY_UNCACHED").get(null);
        Method sampleValue = sampler.getClass()
                .getMethod("sampleValue",
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.SamplerContext"),
                        int.class, int.class, int.class);
        // Several samplers (e.g. InterpolatedFunction$Sampler) are package-private
        // classes with a public method; reflection needs explicit access.
        sampleValue.setAccessible(true);
        return (Float) sampleValue.invoke(sampler, ctxEmpty, x, y, z);
    }

    // ------------------------------------------------------------------ build
    private static MiniJson.Json loadFile(Path p) throws Exception {
        String key = p.toString();
        MiniJson.Json cached = FILES.get(key);
        if (cached != null) return cached;
        if (!Files.exists(p)) throw new IllegalStateException("missing df file " + p);
        MiniJson.Json j = MiniJson.parse(Files.readString(p, StandardCharsets.UTF_8));
        FILES.put(key, j);
        return j;
    }

    private static Object buildRef(String id) throws Exception {
        Object cached = FNS.get(id);
        if (cached != null) return cached;
        Path file = Path.of(".local/df/" + id + ".json");
        MiniJson.Json j = loadFile(file);
        Object fn = buildOp(j);
        FNS.put(id, fn);
        return fn;
    }

    private static Object buildOp(MiniJson.Json j) throws Exception {
        String type = typeOf(j);
        switch (type) {
            case "noise": return buildNoiseFunction(j);
            case "shift_a":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction$ShiftA")
                        .getConstructor(Class.forName("net.minecraft.core.Holder"))
                        .newInstance(holderFor(j.strVal("noise")));
            case "shift_b":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction$ShiftB")
                        .getConstructor(Class.forName("net.minecraft.core.Holder"))
                        .newInstance(holderFor(j.strVal("noise")));
            case "add": return binary("ADD", j);
            case "sub": return binary("SUB", j);
            case "mul": return binary("MUL", j);
            case "div": return binary("DIV", j);
            case "min": return binary("MIN", j);
            case "max": return binary("MAX", j);
            case "abs": return unary("ABS", j);
            case "square": return unary("SQUARE", j);
            case "cube": return unary("CUBE", j);
            case "half_negative": return unary("HALF_NEGATIVE", j);
            case "quarter_negative": return unary("QUARTER_NEGATIVE", j);
            case "negate": return unary("NEGATE", j);
            case "squeeze": return unary("SQUEEZE", j);
            case "log": return unary("LOG", j);
            case "sign": return unary("SIGN", j);
            case "cache":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.CacheFunction")
                        .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                        .newInstance(child(j, "input"));
            case "clamp":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.ClampFunction")
                        .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"), float.class, float.class)
                        .newInstance(child(j, "input"), (float) j.numVal("min", 0), (float) j.numVal("max", 0));
            case "lerp":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.LerpFunction")
                        .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                        .newInstance(child(j, "alpha"), child(j, "first"), child(j, "second"));
            case "range_choice":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.RangeChoiceFunction")
                        .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                float.class, float.class,
                                Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                        .newInstance(child(j, "input"), (float) j.numVal("min_inclusive", 0), (float) j.numVal("max_exclusive", 0),
                                child(j, "when_in_range"), child(j, "when_out_of_range"));
            case "interpolated":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.InterpolatedFunction")
                        .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"), int.class, int.class)
                        .newInstance(child(j, "input"), j.intVal("cell_size_xz", 0), j.intVal("cell_size_y", 0));
            case "find_top_surface":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.FindTopSurfaceFunction")
                        .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                int.class, int.class)
                        .newInstance(child(j, "density"),
                                j.find("upper_bound") != null ? child(j, "upper_bound") : child(j, "density"),
                                j.intVal("lower_bound", -64), j.intVal("cell_height", 0));
            case "y_clamped_gradient":
            case "gradient":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.GradientFunction")
                        .getConstructor(Class.forName("net.minecraft.core.Direction$Axis"),
                                Class.forName("net.minecraft.world.level.levelgen.densityfunction.TilingMode"),
                                int.class, int.class, float.class, float.class)
                        .newInstance(axisOf(j.strVal("axis")), tilingOf(j.strVal("tiling")),
                                j.intVal("from_coordinate", 0), j.intVal("to_coordinate", 0),
                                (float) j.numVal("from_value", 0), (float) j.numVal("to_value", 0));
            case "constant":
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction")
                        .getConstructor(float.class).newInstance((float) j.numVal("value", 0));
            case "beardifier":
            case "blend_alpha":
            case "blend_offset":
                // Codecs map these to SimpleDensityFunction constants via
                // DensityFunctions.*, not the Beardifier helper class.
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunctions")
                        .getMethod(type.equals("beardifier") ? "beardifier"
                                : type.equals("blend_alpha") ? "blendAlpha" : "blendOffset")
                        .invoke(null);
            case "blend_density":
            case "distance_to_point":
            case "old_blended_noise":
            case "interval_select":
            case "slice":
            case "spline": {
                // Vanilla implementations as built by DensityFunctions.* / codecs.
                // C++ mirrors them in src/df.cpp; parity diffs drive fixes.
                Class<?> c;
                Object a, b, d;
                switch (type) {
                    case "blend_density":
                        c = Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.BlendDensityFunction");
                        return c.getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                                .newInstance(child(j, "input"));
                    case "distance_to_point":
                        c = Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.DistanceToPointFunction");
                        a = j.strVal("radius");
                        return c.getConstructor(int.class, double.class, double.class, double.class)
                                .newInstance(j.intVal("inclusive", 0), j.numVal("y", 0), j.numVal("x", 0), j.numVal("z", 0));
                    case "old_blended_noise":
                        c = Class.forName("net.minecraft.world.level.levelgen.synth.BlendedNoise");
                        return c.getConstructor(double.class, double.class, double.class,
                                        double.class, double.class)
                                .newInstance(j.numVal("xz_scale", 0.25), j.numVal("y_scale", 0.0),
                                        j.numVal("xz_factor", 0.15), j.numVal("y_factor", 0.65),
                                        j.numVal("smear_scale_multiplier", 4.0));
                    case "interval_select": {
                        // 26.3 IntervalSelectFunction(DensityFunction input, FloatList thresholds,
                        // List<DensityFunction> functions). Sampler: first threshold > input wins,
                        // otherwise the last function. Verified in IntervalSelectFunction$Sampler.
                        c = Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.IntervalSelectFunction");
                        // Codec field order is input -> thresholds -> functions
                        // (IntervalSelectFunction.txt), so the input child is built first;
                        // that order decides the shared-RNG noise seeds.
                        Object inputFn = buildValue(j.find("input"));
                        MiniJson.Json th = j.find("thresholds");
                        if (th == null || th.type != MiniJson.Type.ARR) throw new IllegalStateException("interval_select missing thresholds: " + j.dump());
                        float[] thresholds = new float[th.arr.size()];
                        for (int i = 0; i < thresholds.length; i++) thresholds[i] = (float) th.arr.get(i).num;
                        Object list = Class.forName("it.unimi.dsi.fastutil.floats.FloatArrayList")
                                .getConstructor(float[].class).newInstance((Object) thresholds);
                        List<Object> functions = new ArrayList<>();
                        MiniJson.Json fs = j.find("functions");
                        if (fs == null || fs.type != MiniJson.Type.ARR) throw new IllegalStateException("interval_select missing functions: " + j.dump());
                        for (MiniJson.Json f : fs.arr) functions.add(buildValue(f));
                        return c.getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                        Class.forName("it.unimi.dsi.fastutil.floats.FloatList"), java.util.List.class)
                                .newInstance(inputFn, list, functions);
                    }
                    case "slice": {
                        c = Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.SliceFunction");
                        // SliceFunction: slices on a fixed coordinate; JSON may use axis+fixed or x/z directly.
                        Class<?> axisC = Class.forName("net.minecraft.core.Direction$Axis");
                        Object axis = axisOf(j.strVal("axis"));
                        double fixed = j.numVal("fixed_value", j.numVal("from_value", 0));
                        return c.getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                                        axisC, double.class, double.class, double.class, double.class)
                                .newInstance(child(j, "input"), axis, fixed,
                                        j.numVal("from_min", 0), j.numVal("from_max", 0), j.numVal("to_value", 0));
                    }
                    case "spline":
                        // SplineFunction wraps a CubicSpline<Coordinate>; build the real
                        // one through CubicSpline.builder + Builder.addPoint so the
                        // oracle runs vanilla's cubic interpolation, not a fallback.
                        c = Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.SplineFunction");
                        return c.getConstructor(Class.forName("net.minecraft.util.CubicSpline"))
                                .newInstance(buildSpline(j.find("spline")));
                    default:
                        throw new UnsupportedOperationException("df op " + type);
                }
            }
            default:
                throw new UnsupportedOperationException("df op " + type + " keys=" + j.obj.keySet());
        }
    }

    private static Object buildNoiseFunction(MiniJson.Json j) throws Exception {
        String noiseId = j.strVal("noise");
        Object normalNoise = parseNormalNoise(noiseId);
        synthFor(normalNoise); // creates the Noise; SYNTH_BY_NOISE maps NormalNoise -> Noise
        Object holder = Class.forName("net.minecraft.core.Holder").getMethod("direct", Object.class).invoke(null, normalNoise);
        double xz = j.find("xz_scale") == null ? 0.25 : j.numVal("xz_scale", 0.25);
        double y = j.find("y_scale") == null ? 0.0 : j.numVal("y_scale", 0.0);
        return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.NoiseFunction")
                .getConstructor(Class.forName("net.minecraft.core.Holder"), double.class, double.class,
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                .newInstance(holder, xz, y,
                        j.find("shift_x") != null ? child(j, "shift_x")
                                : Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction")
                                        .getConstructor(float.class).newInstance(0.0f),
                        j.find("shift_y") != null ? child(j, "shift_y")
                                : Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction")
                                        .getConstructor(float.class).newInstance(0.0f),
                        j.find("shift_z") != null ? child(j, "shift_z")
                                : Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction")
                                        .getConstructor(float.class).newInstance(0.0f));
    }

    // ------------------------------------------------------------------ helpers
    private static String typeOf(MiniJson.Json j) {
        String s = j.strVal("type");
        return s == null ? "" : (s.startsWith("minecraft:") ? s.substring(10) : s);
    }

    /** JSON spline -> real CubicSpline<Coordinate> via CubicSpline.builder + addPoint. */
    private static Object buildSpline(MiniJson.Json spline) throws Exception {
        if (spline == null) throw new IllegalStateException("spline node missing");
        Class<?> coordinateC = Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.SplineFunction$Coordinate");
        Object coordinate = coordinateC.getConstructor(
                Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                .newInstance(buildValue(spline.find("coordinate")));
        Class<?> cubicC = Class.forName("net.minecraft.util.CubicSpline");
        Object builder = cubicC.getMethod("builder", Class.forName("net.minecraft.util.BoundedFloatFunction"))
                .invoke(null, coordinate);
        Class<?> builderC = Class.forName("net.minecraft.util.CubicSpline$Builder");
        MiniJson.Json points = spline.find("points");
        if (points == null || points.type != MiniJson.Type.ARR) {
            throw new IllegalStateException("spline has no points: " + spline.dump());
        }
        for (MiniJson.Json p : points.arr) {
            float location = (float) p.numVal("location", 0);
            float derivative = (float) p.numVal("derivative", 0);
            MiniJson.Json v = p.find("value");
            if (v == null) throw new IllegalStateException("spline point missing value: " + p.dump());
            if (v.type == MiniJson.Type.NUM) {
                builderC.getMethod("addPoint", float.class, float.class, float.class)
                        .invoke(builder, location, (float) v.num, derivative);
            } else {
                // Nested spline points go through the public 2-arg overload, which
                // vanilla forces to derivative 0.0F (see CubicSpline$Builder.txt).
                builderC.getMethod("addPoint", float.class, cubicC)
                        .invoke(builder, location, buildSpline(v));
            }
        }
        return builderC.getMethod("build").invoke(builder);
    }

    private static Object zero() throws Exception {
        return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction")
                .getConstructor(float.class).newInstance(0.0f);
    }
    private static Object child(MiniJson.Json j, String key) throws Exception {
        MiniJson.Json el = j.find(key);
        if (el == null) {
            // Compiler-rewrite paths expect non-null children; default absent
            // children to zero-density constants.
            return zero();
        }
        return buildValue(el);
    }

    private static Object buildValue(MiniJson.Json el) throws Exception {
        if (el.type == MiniJson.Type.STR) {
            String s = el.str;
            if (s.startsWith("minecraft:")) return buildRef(s.substring(10));
            try {
                return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction")
                        .getConstructor(float.class).newInstance((float) Double.parseDouble(s));
            } catch (NumberFormatException e) {
                return buildRef(s);
            }
        }
        if (el.type == MiniJson.Type.NUM) {
            return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction")
                    .getConstructor(float.class).newInstance((float) el.num);
        }
        if (el.obj.isEmpty()) {
            // Empty placeholder node (codec defaults, e.g. degenerate JSON slots).
            return zero();
        }
        return buildOp(el);
    }

    private static Object binary(String type, MiniJson.Json j) throws Exception {
        return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.BinaryFunction")
                .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.BinaryFunction$Type"),
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                .newInstance(Enum.valueOf((Class<? extends Enum>) Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.BinaryFunction$Type"), type),
                        child(j, "left"), child(j, "right"));
    }

    private static Object unary(String type, MiniJson.Json j) throws Exception {
        return Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.UnaryFunction")
                .getConstructor(Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.UnaryFunction$Type"),
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                .newInstance(Enum.valueOf((Class<? extends Enum>) Class.forName("net.minecraft.world.level.levelgen.densityfunction.op.UnaryFunction$Type"), type),
                        child(j, "input"));
    }

    private static Object axisOf(String axis) throws Exception {
        return Enum.valueOf((Class<? extends Enum>) Class.forName("net.minecraft.core.Direction$Axis"), axis.toUpperCase());
    }

    private static Object tilingOf(String tiling) throws Exception {
        if (tiling == null || tiling.isEmpty()) {
            return Enum.valueOf((Class<? extends Enum>) Class.forName("net.minecraft.world.level.levelgen.densityfunction.TilingMode"), "CLAMP_TO_EDGE");
        }
        String name = tiling.startsWith("minecraft:") ? tiling.substring(10) : tiling;
        return Enum.valueOf((Class<? extends Enum>) Class.forName("net.minecraft.world.level.levelgen.densityfunction.TilingMode"),
                name.toUpperCase().replace('-', '_'));
    }

    private static Object parseNormalNoise(String id) throws Exception {
        if (id.startsWith("minecraft:")) id = id.substring(10);
        // RandomState.getOrCreateNoise: one NormalNoise per id, shared by every
        // density function that references it (so shift_x and shift_z share one).
        Object cachedNormal = NORMAL_BY_ID.get(id);
        if (cachedNormal != null) return cachedNormal;
        System.err.println("NOISE " + id);
        Path file = Path.of(".local/noise/" + id + ".json");
        MiniJson.Json j = MiniJson.parse(Files.readString(file, StandardCharsets.UTF_8));
        Class<?> builder = Class.forName("net.minecraft.world.level.levelgen.synth.NormalNoise$Builder");
        Object b = Class.forName("net.minecraft.world.level.levelgen.synth.NormalNoise").getMethod("builder").invoke(null);
        builder.getMethod("setBaseOctave", int.class).invoke(b, j.intVal("base_octave", 0));
        builder.getMethod("setBaseAmplitude", double.class).invoke(b, j.numVal("base_amplitude", 1.0));
        if (j.find("octave_count") != null) builder.getMethod("setOctaveCount", int.class).invoke(b, j.intVal("octave_count", 0));
        if (j.find("amplitude_modifiers") != null) {
            Method setMod = builder.getMethod("setAmplitudeModifier", int.class, double.class);
            List<MiniJson.Json> arr = j.find("amplitude_modifiers").arr;
            for (int i = 0; i < arr.size(); i++) {
                double v = arr.get(i).num;
                if (v != 1.0) setMod.invoke(b, i, v);
            }
        }
        Object built = builder.getMethod("build").invoke(b);
        NORMAL_BY_ID.put(id, built);
        return built;
    }
}
