import java.io.PrintWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

/**
 * CraftEngine Java oracle: the real 26.3 noise.
 *
 * The local JDK is 21 and the 26.3 jar is class-file version 69, so this uses
 * reflection only (no compile-time Mojang/gson imports). It builds the real
 * NormalNoise through its public Builder, with the parameters from the same
 * JSON files the game reads, and samples with the real XoroshiroRandomSource.
 *
 * The C++ probe (--probe noise) reads the same JSON and samples the same points.
 */
public final class NoiseOracle {
    private static final String[] NOISES = {
            "continentalness", "erosion", "ridge", "temperature", "vegetation",
            "cave_cheese", "aquifer_barrier", "surface", "jagged", "noodle",
    };
    private static final long[] SEEDS = {0L, 42L, 1337L, -1L};

    private static final StringBuilder OUT = new StringBuilder(1 << 16);
    private static boolean first = true;

    private static String floatBits(float f) {
        return Integer.toUnsignedString(Float.floatToRawIntBits(f));
    }

    private static String doubleBits(double d) {
        return Long.toUnsignedString(Double.doubleToRawLongBits(d));
    }

    private static void entry(String name, long seed, List<String> values) {
        if (!first) OUT.append(",\n");
        first = false;
        OUT.append("    {\"name\":\"").append(name).append("\",\"seed\":\"").append(seed)
           .append("\",\"values\":[");
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) OUT.append(',');
            OUT.append('"').append(values.get(i)).append('"');
        }
        OUT.append("]}");
    }

    private static Object makeRandom(long seed) throws Exception {
        Class<?> c = Class.forName("net.minecraft.world.level.levelgen.XoroshiroRandomSource");
        Constructor<?> k = c.getConstructor(long.class);
        return k.newInstance(seed);
    }

    private static Object makePerlin(long seed) throws Exception {
        Class<?> c = Class.forName("net.minecraft.world.level.levelgen.synth.PerlinNoise");
        Constructor<?> k = c.getConstructor(Class.forName("net.minecraft.util.RandomSource"));
        return k.newInstance(makeRandom(seed));
    }

    private static float perlinGet(Object perlin, double x, double y, double z)
            throws Exception {
        return (Float) perlin.getClass()
                .getMethod("get", double.class, double.class, double.class)
                .invoke(perlin, x, y, z);
    }

    private static void runPerlin(long seed) throws Exception {
        Object perlin = makePerlin(seed);

        List<String> offsets = new ArrayList<>();
        Class<?> gradient = Class.forName("net.minecraft.world.level.levelgen.synth.GradientNoise");
        for (String field : new String[]{"offsetX", "offsetY", "offsetZ"}) {
            java.lang.reflect.Field f = gradient.getDeclaredField(field);
            f.setAccessible(true);
            offsets.add(doubleBits(f.getDouble(perlin)));
        }
        entry("perlin-offsets:" + seed, seed, offsets);

        List<String> values = new ArrayList<>();
        for (int i = 0; i < 96; i++) {
            double[] p = point3d(i);
            values.add(floatBits(perlinGet(perlin, p[0], p[1], p[2])));
        }
        entry("perlin3d:" + seed, seed, values);

        values.clear();
        for (int i = 0; i < 64; i++) {
            double[] p = point2d(i);
            values.add(floatBits(perlinGet(perlin, p[0], 0.0, p[1])));
        }
        entry("perlin2d:" + seed, seed, values);
    }

    // ---- tiny JSON reader for the four noise parameters -------------------
    private static String fieldText(String text, String key) {
        int i = text.indexOf("\"" + key + "\"");
        if (i < 0) return null;
        int colon = text.indexOf(':', i);
        int j = colon + 1;
        while (j < text.length() && Character.isWhitespace(text.charAt(j))) j++;
        int end = j;
        if (text.charAt(j) == '[') {
            int depth = 0;
            while (end < text.length()) {
                char c = text.charAt(end);
                if (c == '[') depth++;
                if (c == ']') { depth--; if (depth == 0) { end++; break; } }
                end++;
            }
        } else {
            while (end < text.length() && ",}\r\n \t".indexOf(text.charAt(end)) < 0) end++;
        }
        return text.substring(j, end);
    }

    private static double number(String text, String key, double fallback) {
        String s = fieldText(text, key);
        return s == null ? fallback : Double.parseDouble(s.trim());
    }

    private static int integer(String text, String key, int fallback) {
        String s = fieldText(text, key);
        return s == null ? fallback : (int) Double.parseDouble(s.trim());
    }

    private static double[] numbers(String text, String key) {
        String s = fieldText(text, key);
        if (s == null) return new double[0];
        s = s.trim();
        s = s.substring(1, s.length() - 1).trim();
        if (s.isEmpty()) return new double[0];
        String[] parts = s.split(",");
        double[] out = new double[parts.length];
        for (int i = 0; i < parts.length; i++) out[i] = Double.parseDouble(parts[i].trim());
        return out;
    }

    private static Object buildNormal(String text) throws Exception {
        Class<?> normalClass = Class.forName("net.minecraft.world.level.levelgen.synth.NormalNoise");
        Object builder = normalClass.getMethod("builder").invoke(null);
        Class<?> builderClass = builder.getClass();

        builderClass.getMethod("setBaseOctave", int.class)
                .invoke(builder, integer(text, "base_octave", 0));
        builderClass.getMethod("setBaseAmplitude", double.class)
                .invoke(builder, number(text, "base_amplitude", 1.0));

        double[] mods = numbers(text, "amplitude_modifiers");
        int octaveCount = integer(text, "octave_count", mods.length == 0 ? 1 : mods.length);
        builderClass.getMethod("setOctaveCount", int.class).invoke(builder, octaveCount);

        Method setModifier = builderClass.getMethod("setAmplitudeModifier", int.class, double.class);
        for (int i = 0; i < mods.length; i++) {
            if (mods[i] != 1.0) setModifier.invoke(builder, i, mods[i]);
        }
        return builderClass.getMethod("build").invoke(builder);
    }

    private static void runNormal(String name, Path file, long seed) throws Exception {
        String text = Files.readString(file, StandardCharsets.UTF_8);
        Object normalNoise = buildNormal(text);

        Object random = makeRandom(seed);
        Object noise = normalNoise.getClass()
                .getMethod("create", Class.forName("net.minecraft.util.RandomSource"))
                .invoke(normalNoise, random);

        List<String> values = new ArrayList<>();
        Method get = Class.forName("net.minecraft.world.level.levelgen.synth.Noise")
                .getMethod("get", double.class, double.class, double.class);
        for (int i = 0; i < 128; i++) {
            double[] p = point3d(i);
            values.add(floatBits((Float) get.invoke(noise, p[0], p[1], p[2])));
        }
        entry("normal3d:" + name + ":" + seed, seed, values);
    }

    private static double[] point3d(int i) {
        return new double[]{
                (i % 8) * 37.5 - 100.0,
                ((i * 7) % 37) * 3.0 - 64.0,
                (i / 8) * 23.25 + 50.0,
        };
    }

    private static double[] point2d(int i) {
        return new double[]{i * 3.7 - 50.0, i * 2.9 + 7.0};
    }

    public static void main(String[] args) throws Exception {
        String outPath = args.length > 0 ? args[0] : "noise.json";
        Path noiseDir = Path.of(args.length > 1 ? args[1] : ".local/noise");

        OUT.append("{\n  \"version\":\"26.3\",\n  \"noise\":[\n");
        for (long seed : SEEDS) runPerlin(seed);
        for (String name : NOISES) {
            Path file = noiseDir.resolve(name + ".json");
            if (!Files.exists(file)) {
                System.err.println("missing noise file: " + file);
                continue;
            }
            for (long seed : SEEDS) runNormal(name, file, seed);
        }
        OUT.append("\n  ]\n}\n");

        try (PrintWriter w = new PrintWriter(outPath, StandardCharsets.UTF_8)) {
            w.print(OUT);
        }
        System.out.println("wrote " + outPath + " (" + OUT.length() + " bytes)");
    }
}
