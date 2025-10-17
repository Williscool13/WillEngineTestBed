//
// Created by William on 2025-10-16.
//

#ifndef WILLENGINETESTBED_AUDIO_H
#define WILLENGINETESTBED_AUDIO_H

#include <thread>
#include <SDL3/SDL.h>

namespace Audio
{
class Audio
{
public:
    Audio() = default;



    void Init();

    void Update();

    void Cleanup();

    static void AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total);

private:
    SDL_Window* window{};
    SDL_AudioStream *stream{};
    int32_t currentSineSample{0};

    float* pDecodedInterleavedPCMFrames;
    size_t sampleCount;
    size_t sampleSize;

    std::atomic<bool> bShouldExit{false};
};
} // Audio

#endif //WILLENGINETESTBED_AUDIO_H