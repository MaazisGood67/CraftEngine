// CraftEngine - runtime vanilla 26.3 data.
//
// Everything here comes from the user's own installation: the registry dump
// produced by tools/dump-vanilla.ps1 (real BuiltInRegistries ids) and vanilla's
// own options.txt (real key bindings, including the user's remaps). Nothing is
// copied into the repository; the generated files live in vanilla/ (gitignored).
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

struct RegistryEntry {
    int id = -1;
    std::string key;   // "minecraft:stone"
    std::string cls;   // "net.minecraft.world.level.block.Block"
};

class VanillaData {
public:
    // projectDir: folder holding vanilla/registry.json + vanilla/options.txt.
    bool load(const std::string& projectDir, std::string& err);

    bool loaded() const { return loaded_; }
    const std::string& version() const { return version_; }

    const std::vector<RegistryEntry>* registry(const std::string& name) const;
    const RegistryEntry* entry(const std::string& registryName, const std::string& key) const;
    int idOf(const std::string& registryName, const std::string& key) const;

    bool blockIsAir(const std::string& key) const;
    int blockLight(const std::string& key) const;
    int itemMaxStack(const std::string& key) const;
    int itemDurability(const std::string& key) const;

    // Vanilla KeyMapping names ("key.forward"). Returns 0 when unbound/unknown.
    int key(const std::string& name, int fallback = 0) const;
    // Mouse binding: -1 when the mapping is not a mouse button.
    int mouseButton(const std::string& name) const;
    bool hasBind(const std::string& name) const;

    size_t registryCount() const { return registries_.size(); }
    size_t totalEntries() const { return totalEntries_; }
    size_t bindCount() const { return binds_.size(); }

private:
    struct Bind {
        std::string name;      // "key.forward"
        std::string value;     // "key.keyboard.w"
        int vk = 0;            // 0 = unbound
        int mouse = -1;        // 0 left, 1 right, 2 middle, ...
    };

    std::map<std::string, std::vector<RegistryEntry>> registries_;
    std::map<std::string, std::map<std::string, int>> ids_;
    std::map<std::string, Bind> binds_;
    std::map<std::string, bool> blockAir_;
    std::map<std::string, int> blockLight_;
    std::map<std::string, int> itemStack_;
    std::map<std::string, int> itemDurability_;
    std::string version_ = "26.3";
    size_t totalEntries_ = 0;
    bool loaded_ = false;
};

// Parses a vanilla key name ("key.keyboard.left.shift", "key.mouse.middle").
// Returns the VK code (0 for unknown) and sets mouseButton to >= 0 for mouse.
int ParseVanillaKeyName(const std::string& name, int& mouseButton);
