// CraftEngine - asset access: client jar + Mojang asset-index object store.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "zip.h"

class Assets {
public:
    // Locates %APPDATA%\Migurinth, loads 26.3.jar and the asset index.
    bool init(std::string& err);

    bool has(const std::string& name) const;
    // name like "minecraft/textures/font/ascii.png" (no "assets/" prefix).
    std::vector<uint8_t> read(const std::string& name) const;
    std::string readText(const std::string& name) const;

    const std::string& lang(const std::string& key) const;
    const std::vector<std::string>& splashes() const { return splashes_; }
    const std::string& jarPath() const { return jarPath_; }
    const std::string& versionId() const { return versionId_; }
    const std::vector<ZipEntry>& entries() const { return jar_; }

private:
    std::vector<ZipEntry> jar_;
    std::map<std::string, std::string> index_; // asset name -> sha1
    std::map<std::string, std::string> lang_;
    std::vector<std::string> splashes_;
    std::string objectsRoot_;
    std::string jarPath_;
    std::string versionId_ = "26.3";
    mutable std::string empty_;
};

std::string ReadFileBytes(const std::string& path);
std::string AppDataRoaming();
