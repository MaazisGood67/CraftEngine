// CraftEngine - minimal JSON parser (assets index, lang, version metadata).
#pragma once
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;

    const Json* find(const std::string& key) const {
        if (type != Object) return nullptr;
        auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }
    const Json* at(size_t i) const {
        if (type != Array || i >= arr.size()) return nullptr;
        return &arr[i];
    }
    std::string asString() const { return type == String ? str : std::string(); }
};

namespace json_detail {

struct Parser {
    const char* p;
    const char* end;
    int depth = 0;

    void skipWs() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }

    bool parseValue(Json& out) {
        if (depth > 200) return false;
        skipWs();
        if (p >= end) return false;
        switch (*p) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': out.type = Json::String; return parseString(out.str);
            case 't':
                if (end - p >= 4 && std::string(p, 4) == "true") { p += 4; out.type = Json::Bool; out.b = true; return true; }
                return false;
            case 'f':
                if (end - p >= 5 && std::string(p, 5) == "false") { p += 5; out.type = Json::Bool; out.b = false; return true; }
                return false;
            case 'n':
                if (end - p >= 4 && std::string(p, 4) == "null") { p += 4; out.type = Json::Null; return true; }
                return false;
            default: {
                char* stop = nullptr;
                const double d = std::strtod(p, &stop);
                if (stop == p) return false;
                p = stop;
                out.type = Json::Number;
                out.num = d;
                return true;
            }
        }
    }

    static void appendUtf8(std::string& s, unsigned cp) {
        if (cp < 0x80) s += static_cast<char>(cp);
        else if (cp < 0x800) {
            s += static_cast<char>(0xC0 | (cp >> 6));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            s += static_cast<char>(0xE0 | (cp >> 12));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            s += static_cast<char>(0xF0 | (cp >> 18));
            s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool parseHex4(unsigned& out) {
        if (end - p < 4) return false;
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = p[i];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<unsigned>(c - 'A' + 10);
            else return false;
        }
        p += 4;
        return true;
    }

    bool parseString(std::string& out) {
        ++p; // opening quote
        out.clear();
        while (p < end) {
            const char c = *p++;
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (p >= end) return false;
            const char e = *p++;
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!parseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        p += 2;
                        unsigned lo = 0;
                        if (!parseHex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool parseArray(Json& out) {
        ++depth;
        ++p;
        out.type = Json::Array;
        skipWs();
        if (p < end && *p == ']') { ++p; --depth; return true; }
        for (;;) {
            Json v;
            if (!parseValue(v)) return false;
            out.arr.push_back(std::move(v));
            skipWs();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; --depth; return true; }
            return false;
        }
    }

    bool parseObject(Json& out) {
        ++depth;
        ++p;
        out.type = Json::Object;
        skipWs();
        if (p < end && *p == '}') { ++p; --depth; return true; }
        for (;;) {
            skipWs();
            if (p >= end || *p != '"') return false;
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (p >= end || *p != ':') return false;
            ++p;
            Json v;
            if (!parseValue(v)) return false;
            out.obj.emplace(std::move(key), std::move(v));
            skipWs();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; --depth; return true; }
            return false;
        }
    }
};

} // namespace json_detail

inline bool ParseJson(const std::string& text, Json& out) {
    json_detail::Parser parser{text.data(), text.data() + text.size()};
    return parser.parseValue(out);
}
