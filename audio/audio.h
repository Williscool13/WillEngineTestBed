//
// Created by William on 2025-10-16.
//

#ifndef WILLENGINETESTBED_AUDIO_H
#define WILLENGINETESTBED_AUDIO_H

#include <thread>
#include <SDL3/SDL.h>

#include "audio_system.h"

namespace Audio
{
class Audio
{
public:
    Audio() = default;

    void Init();

    void Update();

    void Cleanup();

    /**
     * Runs on a background thread, use carefully.
     * @param userdata
     * @param stream
     * @param additional
     * @param total
     */
    static void AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total);

private:
    SDL_Window* window{};
    SDL_AudioStream *stream{};
    int32_t currentSineSample{0};

    AudioSystem audioSystem{};

    AudioClipHandle gunshot{};
    float volume{1.0f};

    float* pDecodedInterleavedPCMFrames;
    size_t sampleCount;
    size_t sampleSize;

    std::atomic<bool> bShouldExit{false};

    enki::TaskScheduler scheduler;
};
} // Audio

#endif //WILLENGINETESTBED_AUDIO_H