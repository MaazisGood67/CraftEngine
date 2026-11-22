#include "sound.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len, int* channels,
                                        int* sample_rate, short** output);

namespace {

constexpr int kMaxVoices = 4;

} // namespace

bool Sound::init(const Assets* assets) {
    assets_ = assets;
    voices_.resize(kMaxVoices);
    return true;
}

void Sound::shutdown() {
    for (auto& v : voices_) {
        if (v.handle) {
            waveOutReset(v.handle);
            if (v.header.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(v.handle, &v.header, sizeof(WAVEHDR));
            waveOutClose(v.handle);
            v.handle = nullptr;
        }
        v.busy = false;
    }
    voices_.clear();
    cache_.clear();
}

const std::vector<int16_t>* Sound::decoded(const std::string& assetName, int& rate) {
    auto it = cache_.find(assetName);
    if (it != cache_.end()) {
        rate = rates_[assetName];
        return it->second.get();
    }
    if (!assets_) return nullptr;
    const std::vector<uint8_t> ogg = assets_->read(assetName);
    if (ogg.empty()) return nullptr;

    int channels = 0;
    int sampleRate = 0;
    short* output = nullptr;
    const int frames = stb_vorbis_decode_memory(ogg.data(), static_cast<int>(ogg.size()),
                                                 &channels, &sampleRate, &output);
    if (frames <= 0 || !output) {
        if (output) free(output);
        return nullptr;
    }

    auto pcm = std::make_shared<std::vector<int16_t>>();
    if (channels == 1) {
        pcm->resize(static_cast<size_t>(frames) * 2);
        for (int i = 0; i < frames; ++i) {
            (*pcm)[static_cast<size_t>(i) * 2 + 0] = output[i];
            (*pcm)[static_cast<size_t>(i) * 2 + 1] = output[i];
        }
    } else if (channels == 2) {
        pcm->assign(output, output + static_cast<size_t>(frames) * 2);
    } else {
        pcm->resize(static_cast<size_t>(frames) * 2);
        for (int i = 0; i < frames; ++i) {
            (*pcm)[static_cast<size_t>(i) * 2 + 0] = output[static_cast<size_t>(i) * channels];
            (*pcm)[static_cast<size_t>(i) * 2 + 1] = output[static_cast<size_t>(i) * channels + 1];
        }
    }
    free(output);

    rate = sampleRate;
    cache_.emplace(assetName, pcm);
    rates_.emplace(assetName, sampleRate);
    return cache_[assetName].get();
}

void Sound::play(const std::string& assetName, float volume) {
    if (!assets_ || voices_.empty()) return;

    Voice* voice = nullptr;
    for (auto& v : voices_) {
        if (!v.busy) { voice = &v; break; }
    }
    if (!voice) return;

    int rate = 0;
    const std::vector<int16_t>* src = decoded(assetName, rate);
    if (!src || src->empty()) return;

    if (voice->handle) {
        waveOutReset(voice->handle);
        if (voice->header.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(voice->handle, &voice->header, sizeof(WAVEHDR));
        waveOutClose(voice->handle);
        voice->handle = nullptr;
    }

    voice->pcm.assign(src->begin(), src->end());
    const float gain = std::clamp(volume, 0.0f, 1.0f);
    if (gain < 0.999f) {
        for (auto& s : voice->pcm) s = static_cast<int16_t>(static_cast<float>(s) * gain);
    }

    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = static_cast<DWORD>(rate);
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    if (waveOutOpen(&voice->handle, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        voice->handle = nullptr;
        return;
    }

    voice->header = WAVEHDR{};
    voice->header.lpData = reinterpret_cast<LPSTR>(voice->pcm.data());
    voice->header.dwBufferLength = static_cast<DWORD>(voice->pcm.size() * sizeof(int16_t));
    waveOutPrepareHeader(voice->handle, &voice->header, sizeof(WAVEHDR));
    waveOutWrite(voice->handle, &voice->header, sizeof(WAVEHDR));
    voice->busy = true;
}

void Sound::update() {
    for (auto& v : voices_) {
        if (!v.busy || !v.handle) continue;
        if (v.header.dwFlags & WHDR_DONE) {
            waveOutUnprepareHeader(v.handle, &v.header, sizeof(WAVEHDR));
            waveOutClose(v.handle);
            v.handle = nullptr;
            v.busy = false;
        }
    }
}
