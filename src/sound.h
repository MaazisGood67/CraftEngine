// CraftEngine - .ogg (Vorbis) sound engine via waveOut.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include "assets.h"

class Sound {
public:
    bool init(const Assets* assets);
    void shutdown();
    void play(const std::string& assetName, float volume);
    void update();

private:
    struct Voice {
        HWAVEOUT handle = nullptr;
        WAVEHDR header{};
        std::vector<int16_t> pcm;
        bool busy = false;
    };

    const std::vector<int16_t>* decoded(const std::string& assetName, int& rate);

    const Assets* assets_ = nullptr;
    std::vector<Voice> voices_;
    std::map<std::string, std::shared_ptr<std::vector<int16_t>>> cache_;
    std::map<std::string, int> rates_;
};
