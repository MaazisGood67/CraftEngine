#include "vanilla_data.h"

#include <windows.h>

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "json.h"

namespace {

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool StartsWith(const std::string& s, const char* p) {
    const size_t n = std::strlen(p);
    return s.size() >= n && s.compare(0, n, p) == 0;
}

bool EndsWith(const std::string& s, const char* p) {
    const size_t n = std::strlen(p);
    return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
}

// "minecraft:stone" -> "stone" (vanilla accepts both forms in most lookups).
std::string StripNamespace(const std::string& key) {
    const size_t colon = key.find(':');
    return colon == std::string::npos ? key : key.substr(colon + 1);
}

std::string FileText(const std::string& path) {
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

} // namespace

int ParseVanillaKeyName(const std::string& raw, int& mouseButton) {
    mouseButton = -1;
    const std::string name = Lower(raw);

    if (StartsWith(name, "key.mouse.")) {
        const std::string b = name.substr(10);
        if (b == "left") mouseButton = 0;
        else if (b == "right") mouseButton = 1;
        else if (b == "middle") mouseButton = 2;
        else if (StartsWith(b, "button.")) mouseButton = std::atoi(b.c_str() + 7);
        else return 0;
        return 0;
    }
    if (!StartsWith(name, "key.keyboard.")) return 0;

    const std::string k = name.substr(13);
    if (k.empty() || k == "unknown") return 0;
    if (k.size() == 1) {
        const char c = k[0];
        if (c >= 'a' && c <= 'z') return static_cast<int>(std::toupper(static_cast<unsigned char>(c)));
        if (c >= '0' && c <= '9') return static_cast<int>(c);
        return 0;
    }
    if (k.size() >= 2 && k[0] == 'f') {
        const int n = std::atoi(k.c_str() + 1);
        if (n >= 1 && n <= 24) return VK_F1 + (n - 1);
    }
    if (StartsWith(k, "keypad.")) {
        const std::string rest = k.substr(7);
        if (rest.size() == 1 && rest[0] >= '0' && rest[0] <= '9') return VK_NUMPAD0 + (rest[0] - '0');
        if (rest == "decimal") return VK_DECIMAL;
        if (rest == "divide") return VK_DIVIDE;
        if (rest == "multiply") return VK_MULTIPLY;
        if (rest == "subtract") return VK_SUBTRACT;
        if (rest == "add") return VK_ADD;
        if (rest == "enter") return VK_RETURN;
        if (rest == "equal") return VK_OEM_NEC_EQUAL;
        return 0;
    }

    static const struct { const char* name; int vk; } kTable[] = {
        {"space", VK_SPACE}, {"enter", VK_RETURN}, {"escape", VK_ESCAPE},
        {"tab", VK_TAB}, {"backspace", VK_BACK}, {"delete", VK_DELETE},
        {"insert", VK_INSERT}, {"home", VK_HOME}, {"end", VK_END},
        {"page.up", VK_PRIOR}, {"page.down", VK_NEXT},
        {"up", VK_UP}, {"down", VK_DOWN}, {"left", VK_LEFT}, {"right", VK_RIGHT},
        {"caps.lock", VK_CAPITAL}, {"num.lock", VK_NUMLOCK}, {"scroll.lock", VK_SCROLL},
        {"print.screen", VK_SNAPSHOT}, {"pause", VK_PAUSE}, {"menu", VK_APPS},
        {"left.shift", VK_SHIFT}, {"right.shift", VK_SHIFT},
        {"left.control", VK_CONTROL}, {"right.control", VK_CONTROL},
        {"left.alt", VK_MENU}, {"right.alt", VK_MENU},
        {"left.win", VK_LWIN}, {"right.win", VK_RWIN},
        {"left.super", VK_LWIN}, {"right.super", VK_RWIN},
        {"left.meta", VK_LWIN}, {"right.meta", VK_RWIN},
        {"grave.accent", VK_OEM_3}, {"minus", VK_OEM_MINUS}, {"equal", VK_OEM_PLUS},
        {"left.bracket", VK_OEM_4}, {"right.bracket", VK_OEM_6},
        {"backslash", VK_OEM_5}, {"semicolon", VK_OEM_1}, {"apostrophe", VK_OEM_7},
        {"comma", VK_OEM_COMMA}, {"period", VK_OEM_PERIOD}, {"slash", VK_OEM_2},
    };
    for (const auto& e : kTable) {
        if (k == e.name) return e.vk;
    }
    return 0;
}

bool VanillaData::load(const std::string& projectDir, std::string& err) {
    registries_.clear();
    ids_.clear();
    binds_.clear();
    blockAir_.clear();
    blockLight_.clear();
    itemStack_.clear();
    itemDurability_.clear();
    totalEntries_ = 0;
    loaded_ = false;

    std::string regPath = projectDir + "\\vanilla\\registry.json";
    std::string regText = FileText(regPath);
    if (regText.empty()) {
        regPath = projectDir + "\\.local\\vanilla\\registry.json";
        regText = FileText(regPath);
    }
    if (regText.empty()) {
        err = "registry.json missing (run tools/dump-vanilla.ps1)";
        return false;
    }

    Json root;
    if (!ParseJson(regText, root)) {
        err = "registry.json is not valid JSON";
        return false;
    }
    if (const Json* v = root.find("version"); v && v->type == Json::String) version_ = v->str;

    if (const Json* regs = root.find("registries"); regs && regs->type == Json::Object) {
        for (const auto& kv : regs->obj) {
            if (kv.second.type != Json::Array) continue;
            std::vector<RegistryEntry>& out = registries_[kv.first];
            out.reserve(kv.second.arr.size());
            for (const Json& e : kv.second.arr) {
                if (e.type != Json::Object) continue;
                RegistryEntry entry;
                if (const Json* id = e.find("id"); id && id->type == Json::Number)
                    entry.id = static_cast<int>(id->num);
                if (const Json* key = e.find("key"); key && key->type == Json::String)
                    entry.key = key->str;
                if (const Json* cls = e.find("class"); cls && cls->type == Json::String)
                    entry.cls = cls->str;
                if (entry.key.empty()) continue;
                out.push_back(entry);
                ids_[kv.first].emplace(entry.key, entry.id);
                ids_[kv.first].emplace(StripNamespace(entry.key), entry.id);
            }
            totalEntries_ += out.size();
        }
    }

    if (const Json* states = root.find("blockState"); states && states->type == Json::Object) {
        for (const auto& kv : states->obj) {
            if (kv.second.type != Json::Object) continue;
            const Json* air = kv.second.find("air");
            const Json* light = kv.second.find("light");
            blockAir_[kv.first] = air && air->type == Json::Bool && air->b;
            blockLight_[kv.first] = (light && light->type == Json::Number) ? static_cast<int>(light->num) : 0;
        }
    }
    if (const Json* items = root.find("itemProps"); items && items->type == Json::Object) {
        for (const auto& kv : items->obj) {
            if (kv.second.type != Json::Object) continue;
            if (const Json* stack = kv.second.find("stack"); stack && stack->type == Json::Number)
                itemStack_[kv.first] = static_cast<int>(stack->num);
            if (const Json* dur = kv.second.find("durability"); dur && dur->type == Json::Number)
                itemDurability_[kv.first] = static_cast<int>(dur->num);
        }
    }

    // Vanilla's own options.txt is the authority for key bindings, including
    // any remaps the user made. Defaults below only fill gaps when it is absent.
    std::string optText = FileText(projectDir + "\\vanilla\\options.txt");
    if (optText.empty()) optText = FileText(projectDir + "\\.local\\vanilla\\options.txt");
    size_t start = 0;
    while (start < optText.size()) {
        size_t nl = optText.find('\n', start);
        if (nl == std::string::npos) nl = optText.size();
        std::string line = optText.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (StartsWith(line, "key_")) {
            const size_t colon = line.find(':', 4);
            if (colon != std::string::npos && colon > 4) {
                Bind b;
                b.name = line.substr(4, colon - 4);
                b.value = line.substr(colon + 1);
                b.vk = ParseVanillaKeyName(b.value, b.mouse);
                binds_[b.name] = b;
            }
        }
        start = nl + 1;
    }

    static const struct { const char* name; const char* value; } kDefaults[] = {
        {"key.attack", "key.mouse.left"}, {"key.use", "key.mouse.right"},
        {"key.forward", "key.keyboard.w"}, {"key.left", "key.keyboard.a"},
        {"key.back", "key.keyboard.s"}, {"key.right", "key.keyboard.d"},
        {"key.jump", "key.keyboard.space"}, {"key.sneak", "key.keyboard.left.shift"},
        {"key.sprint", "key.keyboard.left.control"}, {"key.drop", "key.keyboard.q"},
        {"key.inventory", "key.keyboard.e"}, {"key.chat", "key.keyboard.t"},
        {"key.playerlist", "key.keyboard.tab"}, {"key.pickItem", "key.mouse.middle"},
        {"key.command", "key.keyboard.slash"}, {"key.friends", "key.keyboard.o"},
        {"key.socialInteractions", "key.keyboard.p"}, {"key.toggleGui", "key.keyboard.f1"},
        {"key.toggleSpectatorShaderEffects", "key.keyboard.f4"},
        {"key.screenshot", "key.keyboard.f2"}, {"key.togglePerspective", "key.keyboard.f5"},
        {"key.fullscreen", "key.keyboard.f11"}, {"key.swapOffhand", "key.keyboard.f"},
        {"key.saveToolbarActivator", "key.keyboard.c"},
        {"key.loadToolbarActivator", "key.keyboard.x"},
        {"key.advancements", "key.keyboard.l"}, {"key.quickActions", "key.keyboard.g"},
        {"key.debug.overlay", "key.keyboard.f3"}, {"key.debug.modifier", "key.keyboard.f3"},
        {"key.hotbar.1", "key.keyboard.1"}, {"key.hotbar.2", "key.keyboard.2"},
        {"key.hotbar.3", "key.keyboard.3"}, {"key.hotbar.4", "key.keyboard.4"},
        {"key.hotbar.5", "key.keyboard.5"}, {"key.hotbar.6", "key.keyboard.6"},
        {"key.hotbar.7", "key.keyboard.7"}, {"key.hotbar.8", "key.keyboard.8"},
        {"key.hotbar.9", "key.keyboard.9"},
    };
    for (const auto& d : kDefaults) {
        if (binds_.find(d.name) != binds_.end()) continue;
        Bind b;
        b.name = d.name;
        b.value = d.value;
        b.vk = ParseVanillaKeyName(b.value, b.mouse);
        binds_[b.name] = b;
    }

    loaded_ = true;
    return true;
}

const std::vector<RegistryEntry>* VanillaData::registry(const std::string& name) const {
    auto it = registries_.find(name);
    if (it == registries_.end()) {
        it = registries_.find("minecraft:" + name);
    }
    return it == registries_.end() ? nullptr : &it->second;
}

const RegistryEntry* VanillaData::entry(const std::string& registryName,
                                        const std::string& key) const {
    const auto* reg = registry(registryName);
    if (!reg) return nullptr;
    for (const auto& e : *reg) {
        if (e.key == key || e.key == "minecraft:" + key) return &e;
    }
    return nullptr;
}

int VanillaData::idOf(const std::string& registryName, const std::string& key) const {
    auto r = ids_.find(registryName);
    if (r == ids_.end()) r = ids_.find("minecraft:" + registryName);
    if (r == ids_.end()) return -1;
    auto it = r->second.find(key);
    if (it == r->second.end()) it = r->second.find("minecraft:" + key);
    if (it == r->second.end()) it = r->second.find(StripNamespace(key));
    return it == r->second.end() ? -1 : it->second;
}

bool VanillaData::blockIsAir(const std::string& key) const {
    auto it = blockAir_.find(key);
    if (it == blockAir_.end()) it = blockAir_.find("minecraft:" + key);
    return it != blockAir_.end() && it->second;
}

int VanillaData::blockLight(const std::string& key) const {
    auto it = blockLight_.find(key);
    if (it == blockLight_.end()) it = blockLight_.find("minecraft:" + key);
    return it == blockLight_.end() ? 0 : it->second;
}

int VanillaData::itemMaxStack(const std::string& key) const {
    auto it = itemStack_.find(key);
    if (it == itemStack_.end()) it = itemStack_.find("minecraft:" + key);
    return it == itemStack_.end() ? 64 : it->second;
}

int VanillaData::itemDurability(const std::string& key) const {
    auto it = itemDurability_.find(key);
    if (it == itemDurability_.end()) it = itemDurability_.find("minecraft:" + key);
    return it == itemDurability_.end() ? 0 : it->second;
}

int VanillaData::key(const std::string& name, int fallback) const {
    auto it = binds_.find(name);
    if (it == binds_.end()) it = binds_.find("key." + name);
    if (it == binds_.end()) return fallback;
    return it->second.vk; // an explicit "unknown" binding stays unbound
}

int VanillaData::mouseButton(const std::string& name) const {
    auto it = binds_.find(name);
    if (it == binds_.end()) it = binds_.find("key." + name);
    return it == binds_.end() ? -1 : it->second.mouse;
}

bool VanillaData::hasBind(const std::string& name) const {
    return binds_.find(name) != binds_.end() || binds_.find("key." + name) != binds_.end();
}
