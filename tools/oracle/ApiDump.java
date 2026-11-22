import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/**
 * Prints the real 26.3 API surface for the classes a port needs. Tooling only:
 * it never writes game code or assets, and it runs against the user's own jar.
 *
 * Usage: java -cp <classpath> ApiDump net.minecraft.world.level.levelgen.XoroshiroRandomSource ...
 */
public final class ApiDump {
    private static String type(Class<?> c) {
        if (c.isArray()) return type(c.getComponentType()) + "[]";
        return c.getName();
    }

    private static String params(Executable e) {
        StringBuilder b = new StringBuilder();
        Class<?>[] ps = e.getParameterTypes();
        for (int i = 0; i < ps.length; i++) {
            if (i > 0) b.append(", ");
            b.append(type(ps[i]));
        }
        return b.toString();
    }

    private static void dump(Class<?> c) {
        System.out.println("=== " + c.getName() + " ===");
        System.out.println("  extends " + (c.getSuperclass() == null ? "-" : c.getSuperclass().getName()));
        for (Class<?> i : c.getInterfaces()) System.out.println("  implements " + i.getName());
        for (Constructor<?> k : c.getDeclaredConstructors()) {
            System.out.println("  ctor " + Modifier.toString(k.getModifiers()) + " (" + params(k) + ")");
        }
        for (Method m : c.getDeclaredMethods()) {
            if (m.isSynthetic()) continue;
            System.out.println("  " + Modifier.toString(m.getModifiers()) + " " + type(m.getReturnType())
                    + " " + m.getName() + "(" + params(m) + ")");
        }
        for (Field f : c.getDeclaredFields()) {
            if (f.isSynthetic()) continue;
            System.out.println("  field " + Modifier.toString(f.getModifiers()) + " " + type(f.getType())
                    + " " + f.getName());
        }
        System.out.println();
    }

    public static void main(String[] args) throws Exception {
        // Registry-backed classes need the vanilla bootstrap first.
        try {
            Class.forName("net.minecraft.SharedConstants").getMethod("tryDetectVersion").invoke(null);
            Class.forName("net.minecraft.server.Bootstrap").getMethod("bootStrap").invoke(null);
        } catch (Throwable t) {
            System.err.println("bootstrap: " + t);
        }
        for (String name : args) {
            try {
                dump(Class.forName(name));
            } catch (Throwable t) {
                System.out.println("=== " + name + " ===\n  ERROR " + t + "\n");
            }
        }
    }
}
