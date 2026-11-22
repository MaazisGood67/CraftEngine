import java.io.File;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * CraftEngine vanilla oracle.
 *
 * Runs against the user's own Minecraft 26.3 jar (never committed) and dumps the
 * real registries and defaults as JSON. Pure reflection, so it compiles with any
 * JDK and runs on the Java 25 runtime the game needs.
 *
 * Usage: java -cp <libraries>;<26.3.jar>;<oracle> VanillaDump <out.json>
 */
public final class VanillaDump {
    private static final StringBuilder OUT = new StringBuilder(1 << 20);

    private static String esc(String s) {
        if (s == null) return "";
        StringBuilder b = new StringBuilder(s.length() + 8);
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '"': b.append("\\\""); break;
                case '\\': b.append("\\\\"); break;
                case '\n': b.append("\\n"); break;
                case '\r': b.append("\\r"); break;
                case '\t': b.append("\\t"); break;
                default:
                    if (c < 0x20) b.append(String.format("\\u%04x", (int) c));
                    else b.append(c);
            }
        }
        return b.toString();
    }

    private static Method findMethod(Class<?> c, String name, Class<?>... params) {
        try { return c.getMethod(name, params); } catch (Throwable t) { return null; }
    }

    private static Iterable<?> iterate(Object registry) throws Exception {
        Method m = findMethod(registry.getClass(), "iterator");
        if (m == null) m = findMethod(Class.forName("net.minecraft.core.Registry"), "iterator");
        Object result = m.invoke(registry);
        if (result instanceof Iterable) return (Iterable<?>) result;
        if (result instanceof java.util.Iterator) {
            List<Object> list = new ArrayList<>();
            java.util.Iterator<?> it = (java.util.Iterator<?>) result;
            while (it.hasNext()) list.add(it.next());
            return list;
        }
        throw new IllegalStateException("registry iterator returned " + result);
    }

    private static String keyOf(Object rootRegistry, Object registry) {
        try {
            Method m = findMethod(Class.forName("net.minecraft.core.Registry"), "getKey", Object.class);
            return String.valueOf(m.invoke(rootRegistry, registry));
        } catch (Throwable t) {
            return "unknown:" + registry.getClass().getSimpleName();
        }
    }

    private static void dumpRegistry(String name, Object registry, Method getId, Method getKey) throws Exception {
        OUT.append("  \"").append(esc(name)).append("\": [\n");
        int index = 0;
        boolean first = true;
        for (Object entry : iterate(registry)) {
            int id = index;
            if (getId != null) {
                try {
                    Object v = getId.invoke(registry, entry);
                    if (v instanceof Integer && ((Integer) v) >= 0) id = (Integer) v;
                } catch (Throwable ignored) { }
            }
            String key;
            try {
                key = String.valueOf(getKey.invoke(registry, entry));
            } catch (Throwable t) {
                key = "unknown:" + index;
            }
            if (!first) OUT.append(",\n");
            first = false;
            OUT.append("    {\"id\":").append(id)
               .append(",\"key\":\"").append(esc(key))
               .append("\",\"class\":\"").append(esc(entry.getClass().getName())).append("\"}");
            index++;
        }
        OUT.append("\n  ]");
    }

    public static void main(String[] args) throws Exception {
        File outFile = new File(args.length > 0 ? args[0] : "registry.json");
        if (outFile.getParentFile() != null) outFile.getParentFile().mkdirs();

        // BuiltInRegistries refuses to register before the vanilla bootstrap runs,
        // so boot the registries first (this also loads the datafixers).
        try {
            Class<?> sc = Class.forName("net.minecraft.SharedConstants");
            sc.getMethod("tryDetectVersion").invoke(null);
        } catch (Throwable t) {
            System.err.println("version detect failed: " + t);
        }
        try {
            Class<?> boot = Class.forName("net.minecraft.server.Bootstrap");
            boot.getMethod("bootStrap").invoke(null);
        } catch (Throwable t) {
            System.err.println("bootstrap failed: " + t);
            t.printStackTrace();
        }
        Class<?> bir = Class.forName("net.minecraft.core.registries.BuiltInRegistries");
        Class<?> regItf = Class.forName("net.minecraft.core.Registry");
        Method getId = findMethod(regItf, "getId", Object.class);
        Method getKey = findMethod(regItf, "getKey", Object.class);

        Object root = null;
        try { root = bir.getField("REGISTRY").get(null); } catch (Throwable ignored) { }

        List<Object[]> registries = new ArrayList<>();
        if (root != null) {
            for (Object reg : iterate(root)) registries.add(new Object[]{keyOf(root, reg), reg});
        } else {
            // Fallback: every public static Registry field of BuiltInRegistries.
            for (Field f : bir.getFields()) {
                if (!Modifier.isStatic(f.getModifiers())) continue;
                if (!regItf.isAssignableFrom(f.getType())) continue;
                try { registries.add(new Object[]{f.getName().toLowerCase(), f.get(null)}); }
                catch (Throwable ignored) { }
            }
        }

        OUT.append("{\n");
        OUT.append("  \"version\": \"").append(esc(System.getProperty("craftengine.version", "26.3"))).append("\",\n");
        OUT.append("  \"registries\": {\n");
        boolean firstReg = true;
        int total = 0;
        for (Object[] pair : registries) {
            String name = (String) pair[0];
            Object reg = pair[1];
            if (!firstReg) OUT.append(",\n");
            firstReg = false;
            dumpRegistry(name, reg, getId, getKey);
            try {
                Method size = findMethod(reg.getClass(), "size");
                if (size != null) total += (Integer) size.invoke(reg);
            } catch (Throwable ignored) { }
        }
        OUT.append("\n  },\n");

        // Block properties that need no level: air flag and light emission.
        OUT.append("  \"blockState\": {\n");
        boolean firstBlock = true;
        if (root != null) {
            Object blockReg = null;
            for (Object[] pair : registries) if ("minecraft:block".equals(pair[0])) blockReg = pair[1];
            if (blockReg != null) {
                for (Object block : iterate(blockReg)) {
                    try {
                        Object state = block.getClass().getMethod("defaultBlockState").invoke(block);
                        Object isAir = state.getClass().getMethod("isAir").invoke(state);
                        Object light = state.getClass().getMethod("getLightEmission").invoke(state);
                        String key = String.valueOf(getKey.invoke(blockReg, block));
                        if (!firstBlock) OUT.append(",\n");
                        firstBlock = false;
                        OUT.append("    \"").append(esc(key)).append("\": {\"air\":").append(isAir)
                           .append(",\"light\":").append(light).append("}");
                    } catch (Throwable ignored) { }
                }
            }
        }
        OUT.append("\n  },\n");

        // Item properties: max stack size, durability.
        OUT.append("  \"itemProps\": {\n");
        boolean firstItem = true;
        if (root != null) {
            Object itemReg = null;
            for (Object[] pair : registries) if ("minecraft:item".equals(pair[0])) itemReg = pair[1];
            if (itemReg != null) {
                for (Object item : iterate(itemReg)) {
                    try {
                        Method stackM = findMethod(item.getClass(), "getMaxStackSize");
                        Method dmgM = findMethod(item.getClass(), "getMaxDamage");
                        String key = String.valueOf(getKey.invoke(itemReg, item));
                        if (!firstItem) OUT.append(",\n");
                        firstItem = false;
                        OUT.append("    \"").append(esc(key)).append("\": {\"stack\":")
                           .append(stackM == null ? 64 : stackM.invoke(item))
                           .append(",\"durability\":").append(dmgM == null ? 0 : dmgM.invoke(item)).append("}");
                    } catch (Throwable ignored) { }
                }
            }
        }
        OUT.append("\n  },\n");

        // Keybind defaults, if the class exposes them before Options is built.
        OUT.append("  \"keybinds\": {\n");
        boolean firstKey = true;
        try {
            Class<?> km = Class.forName("net.minecraft.client.KeyMapping");
            for (Field f : km.getDeclaredFields()) {
                if (!Modifier.isStatic(f.getModifiers())) continue;
                if (!java.util.Map.class.isAssignableFrom(f.getType())) continue;
                f.setAccessible(true);
                Object map = f.get(null);
                if (!(map instanceof java.util.Map)) continue;
                for (Object o : ((java.util.Map<?, ?>) map).values()) {
                    try {
                        Method getName = findMethod(o.getClass(), "getName");
                        Method getDefault = findMethod(o.getClass(), "getDefaultKey");
                        Method getCategory = findMethod(o.getClass(), "getCategory");
                        String name = String.valueOf(getName.invoke(o));
                        String def = String.valueOf(getDefault.invoke(o));
                        String cat = String.valueOf(getCategory.invoke(o));
                        if (!firstKey) OUT.append(",\n");
                        firstKey = false;
                        OUT.append("    \"").append(esc(name)).append("\": {\"key\":\"")
                           .append(esc(def)).append("\",\"category\":\"").append(esc(cat)).append("\"}");
                    } catch (Throwable ignored) { }
                }
            }
        } catch (Throwable ignored) { }
        OUT.append("\n  }\n");

        OUT.append("}\n");

        try (PrintWriter w = new PrintWriter(outFile, StandardCharsets.UTF_8)) {
            w.print(OUT);
        }
        System.out.println("wrote " + outFile.getAbsolutePath() + " (" + OUT.length() + " bytes, " + total + " entries)");
    }
}
