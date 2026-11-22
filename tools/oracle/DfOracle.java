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
    private static final Map<Object, Object> SYNTH_BY_NOISE = new LinkedHashMap<>();
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

    public static void main(String[] args) throws Exception {
        try {
            Class.forName("net.minecraft.SharedConstants").getMethod("tryDetectVersion").invoke(null);
            Class.forName("net.minecraft.server.Bootstrap").getMethod("bootStrap").invoke(null);
        } catch (Throwable t) {
            System.err.println("bootstrap: " + t);
        }
        String outPath = args.length > 0 ? args[0] : "df.json";

        random = newRandom(1337L);

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
        Object synth = normalNoise.getClass()
                .getMethod("create", Class.forName("net.minecraft.util.RandomSource"))
                .invoke(normalNoise, random);
        SYNTH_BY_NOISE.put(normalNoise, synth);
        return synth;
    }

    private static Object holderFor(String id) throws Exception {
        Object normalNoise = synthFor(parseNormalNoise(id));
        return Class.forName("net.minecraft.core.Holder").getMethod("direct", Object.class).invoke(null, normalNoise);
    }

    private static float sample(Object fn, int x, int y, int z) throws Exception {
        Object sampler = fn.getClass()
                .getMethod("compileSampler",
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction$CompileContext"))
                .invoke(fn, CTX);
        Object ctxEmpty = Class.forName("net.minecraft.world.level.levelgen.densityfunction.SamplerContext")
                .getField("EMPTY_UNCACHED").get(null);
        Method sampleValue = sampler.getClass()
                .getMethod("sampleValue",
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.SamplerContext"),
                        int.class, int.class, int.class);
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
                return Class.forName("net.minecraft.world.level.levelgen.Beardifier").getField("EMPTY").get(null);
            default:
                throw new UnsupportedOperationException("df op " + type);
        }
    }

    private static Object buildNoiseFunction(MiniJson.Json j) throws Exception {
        String noiseId = j.strVal("noise");
        Object normalNoise = synthFor(parseNormalNoise(noiseId));
        Object holder = Class.forName("net.minecraft.core.Holder").getMethod("direct", Object.class).invoke(null, normalNoise);
        double xz = j.find("xz_scale") == null ? 0.25 : j.numVal("xz_scale", 0.25);
        double y = j.find("y_scale") == null ? 0.0 : j.numVal("y_scale", 0.0);
        return Class.forName("net.minecraft.world.level.levelgen.densityfunction.generator.NoiseFunction")
                .getConstructor(Class.forName("net.minecraft.core.Holder"), double.class, double.class,
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"),
                        Class.forName("net.minecraft.world.level.levelgen.densityfunction.DensityFunction"))
                .newInstance(holder, xz, y,
                        j.find("shift_x") != null ? child(j, "shift_x") : null,
                        j.find("shift_y") != null ? child(j, "shift_y") : null,
                        j.find("shift_z") != null ? child(j, "shift_z") : null);
    }

    // ------------------------------------------------------------------ helpers
    private static String typeOf(MiniJson.Json j) {
        String s = j.strVal("type");
        return s == null ? "" : (s.startsWith("minecraft:") ? s.substring(10) : s);
    }

    private static Object child(MiniJson.Json j, String key) throws Exception {
        MiniJson.Json el = j.find(key);
        if (el == null) return null;
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
        return builder.getMethod("build").invoke(b);
    }
}
