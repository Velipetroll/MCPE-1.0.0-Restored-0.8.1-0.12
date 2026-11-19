#pragma once

#include <cmath>
#include <cstdint>
#include <vector>
#include <cstdlib>

#ifdef HAS_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#endif

namespace SoundEngine {

enum SoundId {
    SND_CLICK = 0,
    SND_STEP,
    SND_DIG,
    SND_PLACE,
    SND_HURT,
    SND_EXPLODE,
    SND_BOW,
    SND_EAT,
    SND_DOOR,
    SND_COUNT
};

#ifdef HAS_OPENAL
struct State {
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;
    ALuint buffers[SND_COUNT] = {};
    static constexpr int MAX_SOURCES = 16;
    ALuint sources[MAX_SOURCES] = {};
    int nextSource = 0;
    float masterVolume = 1.0f;
    bool initialized = false;
};

inline State& getState() {
    static State s;
    return s;
}

inline void fillBuffer(ALuint buf, const std::vector<int16_t>& samples, int sampleRate = 22050) {
    alBufferData(buf, AL_FORMAT_MONO16, samples.data(),
                 static_cast<ALsizei>(samples.size() * sizeof(int16_t)), sampleRate);
}

inline float randf() {
    return (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
}

inline void generateSounds(State& s) {
    const int SR = 22050;

    // 1. SND_CLICK: short crisp UI click (~35ms)
    {
        int n = SR * 35 / 1000;
        std::vector<int16_t> pcm(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 120.0f);
            float v = std::sin(2.0f * 3.14159265f * 950.0f * t) * 0.5f + randf() * 0.15f;
            pcm[i] = static_cast<int16_t>(v * env * 14000.0f);
        }
        fillBuffer(s.buffers[SND_CLICK], pcm, SR);
    }

    // 2. SND_STEP: soft footstep (~70ms)
    {
        int n = SR * 70 / 1000;
        std::vector<int16_t> pcm(n);
        float lp = 0.0f;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 45.0f);
            lp = lp * 0.8f + randf() * 0.2f;
            float thud = std::sin(2.0f * 3.14159265f * 110.0f * t) * 0.4f;
            pcm[i] = static_cast<int16_t>((lp * 1.5f + thud) * env * 12000.0f);
        }
        fillBuffer(s.buffers[SND_STEP], pcm, SR);
    }

    // 3. SND_DIG: block break crunch (~110ms)
    {
        int n = SR * 110 / 1000;
        std::vector<int16_t> pcm(n);
        float lp = 0.0f;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 28.0f);
            lp = lp * 0.65f + randf() * 0.35f;
            float tone = std::sin(2.0f * 3.14159265f * (160.0f - t * 400.0f) * t) * 0.35f;
            pcm[i] = static_cast<int16_t>((lp + tone) * env * 18000.0f);
        }
        fillBuffer(s.buffers[SND_DIG], pcm, SR);
    }

    // 4. SND_PLACE: block place thud (~80ms)
    {
        int n = SR * 80 / 1000;
        std::vector<int16_t> pcm(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 40.0f);
            float v = std::sin(2.0f * 3.14159265f * (180.0f - t * 600.0f) * t) * 0.7f + randf() * 0.25f;
            pcm[i] = static_cast<int16_t>(v * env * 16000.0f);
        }
        fillBuffer(s.buffers[SND_PLACE], pcm, SR);
    }

    // 5. SND_HURT: hit / damage (~130ms)
    {
        int n = SR * 130 / 1000;
        std::vector<int16_t> pcm(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 24.0f);
            float freq = 260.0f * std::exp(-t * 8.0f);
            float v = std::sin(2.0f * 3.14159265f * freq * t) * 0.6f + randf() * 0.35f;
            pcm[i] = static_cast<int16_t>(v * env * 19000.0f);
        }
        fillBuffer(s.buffers[SND_HURT], pcm, SR);
    }

    // 6. SND_EXPLODE: TNT explosion (~700ms)
    {
        int n = SR * 700 / 1000;
        std::vector<int16_t> pcm(n);
        float lp = 0.0f;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 4.5f);
            lp = lp * 0.85f + randf() * 0.15f;
            float sub = std::sin(2.0f * 3.14159265f * 55.0f * t) * 0.4f;
            float v = (lp * 2.2f + sub) * env;
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            pcm[i] = static_cast<int16_t>(v * 28000.0f);
        }
        fillBuffer(s.buffers[SND_EXPLODE], pcm, SR);
    }

    // 7. SND_BOW: bow twang + arrow whoosh (~180ms)
    {
        int n = SR * 180 / 1000;
        std::vector<int16_t> pcm(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 18.0f);
            float twang = std::sin(2.0f * 3.14159265f * (220.0f + 80.0f * std::exp(-t * 25.0f)) * t) * 0.55f;
            float whoosh = randf() * 0.25f * std::exp(-t * 12.0f);
            pcm[i] = static_cast<int16_t>((twang + whoosh) * env * 18000.0f);
        }
        fillBuffer(s.buffers[SND_BOW], pcm, SR);
    }

    // 8. SND_EAT: crunch (~120ms)
    {
        int n = SR * 120 / 1000;
        std::vector<int16_t> pcm(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 22.0f) * (0.6f + 0.4f * std::sin(2.0f * 3.14159265f * 35.0f * t));
            pcm[i] = static_cast<int16_t>(randf() * env * 14000.0f);
        }
        fillBuffer(s.buffers[SND_EAT], pcm, SR);
    }

    // 9. SND_DOOR: wooden door / fence gate creak-clack (~140ms)
    {
        int n = SR * 140 / 1000;
        std::vector<int16_t> pcm(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / SR;
            float env = std::exp(-t * 20.0f);
            float v = std::sin(2.0f * 3.14159265f * (150.0f + t * 220.0f) * t) * 0.55f + randf() * 0.2f;
            pcm[i] = static_cast<int16_t>(v * env * 15000.0f);
        }
        fillBuffer(s.buffers[SND_DOOR], pcm, SR);
    }
}

inline void init() {
    State& s = getState();
    if (s.initialized) return;
    s.device = alcOpenDevice(nullptr);
    if (!s.device) return;
    s.context = alcCreateContext(s.device, nullptr);
    if (!s.context) {
        alcCloseDevice(s.device);
        s.device = nullptr;
        return;
    }
    alcMakeContextCurrent(s.context);
    alGenBuffers(SND_COUNT, s.buffers);
    alGenSources(State::MAX_SOURCES, s.sources);
    generateSounds(s);
    s.initialized = true;
}

inline void shutdown() {
    State& s = getState();
    if (!s.initialized) return;
    alDeleteSources(State::MAX_SOURCES, s.sources);
    alDeleteBuffers(SND_COUNT, s.buffers);
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(s.context);
    alcCloseDevice(s.device);
    s.context = nullptr;
    s.device = nullptr;
    s.initialized = false;
}

inline void setMasterVolume(float vol) {
    State& s = getState();
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    s.masterVolume = vol;
}

inline void play(SoundId id, float volume = 1.0f, float pitch = 1.0f) {
    State& s = getState();
    if (!s.initialized || id < 0 || id >= SND_COUNT) return;
    float gain = volume * s.masterVolume;
    if (gain <= 0.001f) return;
    ALuint src = s.sources[s.nextSource];
    s.nextSource = (s.nextSource + 1) % State::MAX_SOURCES;
    alSourceStop(src);
    alSourcei(src, AL_BUFFER, static_cast<ALint>(s.buffers[id]));
    alSourcef(src, AL_GAIN, gain);
    alSourcef(src, AL_PITCH, pitch);
    alSourcePlay(src);
}

#else
inline void init() {}
inline void shutdown() {}
inline void setMasterVolume(float) {}
inline void play(SoundId, float = 1.0f, float = 1.0f) {}
#endif

} // namespace SoundEngine
