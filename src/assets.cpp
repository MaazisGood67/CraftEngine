#include "assets.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "json.h"

std::string ReadFileBytes(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string out;
    if (size > 0) {
        out.resize(static_cast<size_t>(size));
        const size_t got = fread(&out[0], 1, static_cast<size_t>(size), f);
        out.resize(got);
    }
    fclose(f);
    return out;
}

std::string AppDataRoaming() {
    const char* appdata = std::getenv("APPDATA");
    return appdata ? std::string(appdata) : std::string();
}

bool Assets::init(std::string& err) {
    const std::string roaming = AppDataRoaming();
    if (roaming.empty()) {
        err = "APPDATA not set";
        return false;
    }
    const std::string root = roaming + "\\Migurinth";

    // Version metadata -> asset index id + jar path.
    const std::string versionJsonPath = root + "\\meta\\versions\\26.3\\26.3.json";
    const std::string versionJson = ReadFileBytes(versionJsonPath);
    std::string indexId = "34";
    if (!versionJson.empty()) {
        Json j;
        if (ParseJson(versionJson, j)) {
            if (const Json* id = j.find("id"); id && id->type == Json::String) versionId_ = id->str;
            if (const Json* ai = j.find("assetIndex")) {
                if (const Json* aid = ai->find("id"); aid && aid->type == Json::String) indexId = aid->str;
            }
        }
    }

    jarPath_ = root + "\\meta\\versions\\26.3\\26.3.jar";
    if (!LoadZip(jarPath_, jar_, err)) {
        err = "jar: " + err;
        return false;
    }

    // Asset index -> name/hash map.
    const std::string indexPath = root + "\\meta\\assets\\indexes\\" + indexId + ".json";
    Json idx;
    const std::string idxText = ReadFileBytes(indexPath);
    if (!idxText.empty() && ParseJson(idxText, idx)) {
        if (const Json* objects = idx.find("objects"); objects && objects->type == Json::Object) {
            for (const auto& kv : objects->obj) {
                if (const Json* hash = kv.second.find("hash"); hash && hash->type == Json::String)
                    index_.emplace(kv.first, hash->str);
            }
        }
    }
    objectsRoot_ = root + "\\meta\\assets\\objects";

    // Language strings (flat string->string map).
    const std::string langText = readText("minecraft/lang/en_us.json");
    if (!langText.empty()) {
        Json langJson;
        if (ParseJson(langText, langJson) && langJson.type == Json::Object) {
            for (const auto& kv : langJson.obj) {
                if (kv.second.type == Json::String) lang_.emplace(kv.first, kv.second.str);
            }
        }
    }

    // Splash lines.
    const std::string splashText = readText("minecraft/texts/splashes.txt");
    {
        size_t start = 0;
        while (start < splashText.size()) {
            size_t nl = splashText.find('\n', start);
            if (nl == std::string::npos) nl = splashText.size();
            std::string line = splashText.substr(start, nl - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) splashes_.push_back(line);
            start = nl + 1;
        }
    }
    return true;
}

bool Assets::has(const std::string& name) const {
    if (index_.find(name) != index_.end()) return true;
    const std::string jarName = "assets/" + name;
    for (const auto& e : jar_) {
        if (e.name == jarName) return true;
    }
    return false;
}

std::vector<uint8_t> Assets::read(const std::string& name) const {
    // Object store first: modern versions ship 1x1 placeholder panorama faces in
    // the jar and the real textures in the asset index. Jar is the fallback.
    auto it = index_.find(name);
    if (it != index_.end()) {
        const std::string& hash = it->second;
        if (hash.size() >= 3) {
            const std::string path = objectsRoot_ + "\\" + hash.substr(0, 2) + "\\" + hash;
            const std::string bytes = ReadFileBytes(path);
            if (!bytes.empty()) return std::vector<uint8_t>(bytes.begin(), bytes.end());
        }
    }
    const std::string jarName = "assets/" + name;
    for (const auto& e : jar_) {
        if (e.name == jarName) return e.data;
    }
    return {};
}

std::string Assets::readText(const std::string& name) const {
    const std::vector<uint8_t> data = read(name);
    return std::string(data.begin(), data.end());
}

const std::string& Assets::lang(const std::string& key) const {
    auto it = lang_.find(key);
    return it == lang_.end() ? empty_ : it->second;
}
