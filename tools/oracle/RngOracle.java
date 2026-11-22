import java.io.PrintWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * CraftEngine Java oracle: the real 26.3 random streams.
 *
 * Runs the shipped XoroshiroRandomSource and LegacyRandomSource through a fixed
 * script and writes every result as an exact string (float/double as raw bits).
 * The C++ probe runs the same script; tools/parity.ps1 compares the two files.
 *
 * Keep the script in sync with tools/probe/probe.cpp (rng section).
 */
public final class RngOracle {
    private static final long[] SEEDS = {
            0L, 1L, 42L, 1337L, -1L,
            9223372036854775807L, -9223372036854775808L,
            1234567890123456789L, -6148914691236517206L,
    };

    private static final StringBuilder OUT = new StringBuilder(1 << 16);
    private static boolean firstEntry = true;

    private static Object make(String cls, long seed) throws Exception {
        Class<?> c = Class.forName(cls);
        Constructor<?> k = c.getConstructor(long.class);
        return k.newInstance(seed);
    }

    private static Object call(Object target, String name, Class<?>[] types, Object... args) throws Exception {
        Method m = target.getClass().getMethod(name, types);
        return m.invoke(target, args);
    }

    private static Object call(Object target, String name) throws Exception {
        return call(target, name, new Class<?>[0]);
    }

    private static String intBits(float f) {
        return Integer.toUnsignedString(Float.floatToRawIntBits(f));
    }

    private static String doubleBits(double d) {
        return Long.toUnsignedString(Double.doubleToRawLongBits(d));
    }

    private static void entry(String gen, long seed, String op, List<String> values) {
        if (!firstEntry) OUT.append(",\n");
        firstEntry = false;
        OUT.append("    {\"gen\":\"").append(gen).append("\",\"seed\":\"").append(seed)
           .append("\",\"op\":\"").append(op).append("\",\"values\":[");
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) OUT.append(',');
            OUT.append('"').append(values.get(i)).append('"');
        }
        OUT.append("]}");
    }

    private static void run(String gen, String cls, long seed) throws Exception {
        Object r = make(cls, seed);
        List<String> v = new ArrayList<>();

        v.clear();
        for (int i = 0; i < 4; i++) v.add(String.valueOf(call(r, "nextInt")));
        entry(gen, seed, "nextInt", v);

        int[] bounds = {16, 1000, 1, 2, 3, 7, 64, 2147483647};
        for (int bound : bounds) {
            v.clear();
            for (int i = 0; i < 4; i++)
                v.add(String.valueOf(call(r, "nextInt", new Class<?>[]{int.class}, bound)));
            entry(gen, seed, "nextInt(" + bound + ")", v);
        }

        v.clear();
        for (int i = 0; i < 3; i++) v.add(String.valueOf(call(r, "nextLong")));
        entry(gen, seed, "nextLong", v);

        v.clear();
        for (int i = 0; i < 3; i++) v.add(String.valueOf(call(r, "nextBoolean")));
        entry(gen, seed, "nextBoolean", v);

        v.clear();
        for (int i = 0; i < 3; i++) v.add(intBits((Float) call(r, "nextFloat")));
        entry(gen, seed, "nextFloatBits", v);

        v.clear();
        for (int i = 0; i < 3; i++) v.add(doubleBits((Double) call(r, "nextDouble")));
        entry(gen, seed, "nextDoubleBits", v);

        v.clear();
        call(r, "consumeCount", new Class<?>[]{int.class}, 3);
        v.add(String.valueOf(call(r, "nextLong")));
        entry(gen, seed, "consume3Long", v);

        v.clear();
        for (int i = 0; i < 3; i++)
            v.add(String.valueOf(call(r, "nextIntBetweenInclusive",
                    new Class<?>[]{int.class, int.class}, -5, 5)));
        entry(gen, seed, "nextIntBetweenInclusive(-5,5)", v);

        v.clear();
        for (int i = 0; i < 2; i++)
            v.add(intBits((Float) call(r, "triangle",
                    new Class<?>[]{float.class, float.class}, 0.0f, 1.0f)));
        entry(gen, seed, "triangleFloatBits", v);

        v.clear();
        for (int i = 0; i < 2; i++)
            v.add(doubleBits((Double) call(r, "triangle",
                    new Class<?>[]{double.class, double.class}, 0.0, 1.0)));
        entry(gen, seed, "triangleDoubleBits", v);

        Object forked = call(r, "fork");
        v.clear();
        v.add(String.valueOf(call(forked, "nextLong")));
        entry(gen, seed, "forkLong", v);
    }

    public static void main(String[] args) throws Exception {
        String outPath = args.length > 0 ? args[0] : "rng.json";
        OUT.append("{\n  \"version\":\"26.3\",\n  \"rng\":[\n");
        for (long seed : SEEDS) {
            run("xoroshiro", "net.minecraft.world.level.levelgen.XoroshiroRandomSource", seed);
            run("legacy", "net.minecraft.world.level.levelgen.LegacyRandomSource", seed);
        }
        OUT.append("\n  ]\n}\n");
        try (PrintWriter w = new PrintWriter(outPath, StandardCharsets.UTF_8)) {
            w.print(OUT);
        }
        System.out.println("wrote " + outPath + " (" + OUT.length() + " bytes)");
    }
}
