import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * CraftEngine MiniJson: minimal JSON parser for the density function files,
 * written with java.* only so javac 21 can compile it (the 26.3 classes in the
 * jar are Java 25, which this JDK cannot read at compile time).
 */
public final class MiniJson {
    public enum Type { NULL, FALSE, TRUE, NUM, STR, ARR, OBJ }

    public static final class Json {
        public Type type = Type.NULL;
        public double num = 0;
        public boolean b;
        public String str = "";
        public final List<Json> arr = new ArrayList<>();
        public final Map<String, Json> obj = new LinkedHashMap<>();

        public Json find(String key) { return obj.get(key); }
        public String strVal(String key) {
            Json v = find(key);
            return v == null ? null : v.str;
        }
        public double numVal(String key, double def) {
            Json v = find(key);
            return v == null ? def : v.num;
        }
        public int intVal(String key, int def) {
            Json v = find(key);
            return v == null ? def : (int) v.num;
        }
    }

    public static Json parse(String text) {
        Parser p = new Parser(text);
        p.skipWs();
        Json v = p.value();
        p.skipWs();
        if (p.i != text.length()) throw new IllegalStateException("trailing data at " + p.i);
        return v;
    }

    private static final class Parser {
        final String s;
        int i = 0;

        Parser(String s) { this.s = s; }

        void skipWs() { while (i < s.length() && (s.charAt(i) == ' ' || s.charAt(i) == '\t' || s.charAt(i) == '\n' || s.charAt(i) == '\r')) i++; }

        char c() { return s.charAt(i); }

        Json value() {
            skipWs();
            char ch = c();
            if (ch == '{') return object();
            if (ch == '[') return array();
            if (ch == '"') { Json v = new Json(); v.type = Type.STR; v.str = string(); return v; }
            if (ch == 't') { expect("true"); Json v = new Json(); v.type = Type.TRUE; return v; }
            if (ch == 'f') { expect("false"); Json v = new Json(); v.type = Type.FALSE; return v; }
            if (ch == 'n') { expect("null"); Json v = new Json(); v.type = Type.NULL; return v; }
            return number();
        }

        Json object() {
            Json o = new Json();
            o.type = Type.OBJ;
            i++; // {
            skipWs();
            if (c() == '}') { i++; return o; }
            for (;;) {
                skipWs();
                String key = string();
                skipWs();
                expectChar(':');
                Json v = value();
                o.obj.put(key, v);
                skipWs();
                if (c() == ',') { i++; continue; }
                expectChar('}');
                return o;
            }
        }

        Json array() {
            Json o = new Json();
            o.type = Type.ARR;
            i++; // [
            skipWs();
            if (c() == ']') { i++; return o; }
            for (;;) {
                o.arr.add(value());
                skipWs();
                if (c() == ',') { i++; continue; }
                expectChar(']');
                return o;
            }
        }

        String string() {
            StringBuilder sb = new StringBuilder();
            expectChar('"');
            while (true) {
                char ch = s.charAt(i++);
                if (ch == '"') break;
                if (ch == '\\') {
                    char e = s.charAt(i++);
                    switch (e) {
                        case '"': sb.append('"'); break;
                        case '\\': sb.append('\\'); break;
                        case '/': sb.append('/'); break;
                        case 'b': sb.append('\b'); break;
                        case 'f': sb.append('\f'); break;
                        case 'n': sb.append('\n'); break;
                        case 'r': sb.append('\r'); break;
                        case 't': sb.append('\t'); break;
                        case 'u':
                            sb.append((char) Integer.parseInt(s.substring(i, i + 4), 16));
                            i += 4;
                            break;
                        default: throw new IllegalStateException("bad escape");
                    }
                } else {
                    sb.append(ch);
                }
            }
            return sb.toString();
        }

        Json number() {
            int start = i;
            if (c() == '-' || c() == '+') i++;
            while (i < s.length() && (Character.isDigit(c()) || c() == '.' || c() == 'e' || c() == 'E'
                    || c() == '-' || c() == '+')) i++;
            Json v = new Json();
            v.type = Type.NUM;
            v.num = Double.parseDouble(s.substring(start, i));
            return v;
        }

        void expect(String lit) { if (!s.startsWith(lit, i)) throw new IllegalStateException("expected " + lit); i += lit.length(); }
        void expectChar(char c) { if (c() != c) throw new IllegalStateException("expected " + c); i++; }
    }
}
