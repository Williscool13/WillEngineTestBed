//
// Created by William on 2025-10-16.
//

#include "audio.h"

#include "crash-handling/logger.h"

namespace Audio
{
void Audio::Init()
{
    // SDL_SetAppMetadata("AUDIO", "1.0", "nothin");
    bool sdlInitSuccess = SDL_Init(SDL_INIT_AUDIO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return;
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Audio Tests",
        800,
        600,
        window_flags);

    SDL_AudioSpec spec{};
    spec.channels = 1;
    spec.format = SDL_AUDIO_F32;
    spec.freq = 8000;

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return;
    }

    SDL_ResumeAudioStreamDevice(stream);
}

void Audio::Update()
{

    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        if (exit) {
            break;
        }

        // Body copied from SDL3 audio examples #1
        const int minimum_audio = (8000 * sizeof (float)) / 2;  /* 8000 float samples per second. Half of that. */
        if (SDL_GetAudioStreamQueued(stream) < minimum_audio) {
            static float samples[512];  /* this will feed 512 samples each frame until we get to our maximum. */
            int i;

            /* generate a 440Hz pure tone */
            for (i = 0; i < SDL_arraysize(samples); i++) {
                const int freq = 440;
                const float phase = currentSineSample * freq / 8000.0f;
                samples[i] = SDL_sinf(phase * 2 * SDL_PI_F);
                currentSineSample++;
            }

            /* wrapping around to avoid floating-point errors */
            currentSineSample %= 8000;

            /* feed the new data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
            SDL_PutAudioStreamData(stream, samples, sizeof (samples));
        }
    }
}
void Audio::Cleanup()
{

}
} // Audio