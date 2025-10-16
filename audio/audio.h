//
// Created by William on 2025-10-16.
//

#ifndef WILLENGINETESTBED_AUDIO_H
#define WILLENGINETESTBED_AUDIO_H

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

private:
    SDL_Window* window{};
    SDL_AudioStream *stream{};
    int32_t currentSineSample{0};
};
} // Audio

#endif //WILLENGINETESTBED_AUDIO_H