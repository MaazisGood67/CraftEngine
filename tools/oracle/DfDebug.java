/**
 * CraftEngine Java oracle debug helper.
 *
 * Prints the sub-values the density harness compares only indirectly, so a
 * mismatch can be attributed to one node (shift chain vs noise vs spline)
 * instead of guessed at. All plumbing is reused from DfOracle, which means the
 * noise seeds here are the ones the harness uses.
 *
 * Usage: java DfDebug <x> <y> <z> [functionId ...]
 *        (defaults to overworld/continents, shift_x, shift_z)
 */
public final class DfDebug {
    public static void main(String[] args) throws Exception {
        if (args.length > 0 && args[0].equals("noises")) {
            DfOracle.init();
            DfOracle.buildWholeRouter();
            DfOracle.debugDumpNoises(0.0, 0.0, 0.0);
            return;
        }
        if (args.length > 0 && args[0].equals("file")) {
            // file <path> <x> <y> <z>: sample an inline DF JSON snippet.
            String path = args[1];
            int fx = Integer.parseInt(args[2]);
            int fy = Integer.parseInt(args[3]);
            int fz = Integer.parseInt(args[4]);
            DfOracle.init();
            DfOracle.buildWholeRouter();
            java.nio.file.Path p = java.nio.file.Path.of(path);
            Object fn = DfOracle.debugBuildValue(
                    MiniJson.parse(java.nio.file.Files.readString(p, java.nio.charset.StandardCharsets.UTF_8)));
            System.out.printf("%s(%d,%d,%d) = %.9g%n", path, fx, fy, fz,
                    DfOracle.debugSample(fn, fx, fy, fz));
            return;
        }
        int x = Integer.parseInt(args[0]);
        int y = Integer.parseInt(args[1]);
        int z = Integer.parseInt(args[2]);

        DfOracle.init();
        DfOracle.buildWholeRouter();

        String[] wanted = args.length > 3 ? java.util.Arrays.copyOfRange(args, 3, args.length)
                : new String[]{"overworld/continents", "shift_x", "shift_z"};
        for (String id : wanted) {
            float v = DfOracle.debugSample(DfOracle.debugBuildRef(id), x, y, z);
            System.out.printf("%s(%d,%d,%d) = %.9g%n", id, x, y, z, v);
        }
    }
}
