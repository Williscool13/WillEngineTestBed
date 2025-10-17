//
// Created by William on 2025-10-16.
//

#include "audio.h"

#include "crash-handling/logger.h"

#include "dr_libs/dr_wav.h"

namespace Audio
{
void SDLCALL Audio::AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total)
{
    auto* self = static_cast<Audio*>(userdata);

    while (additional > 0) {
        /* feed more data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
        SDL_PutAudioStreamData(stream, self->pDecodedInterleavedPCMFrames, self->sampleSize);
        additional -= self->sampleSize;
    }
}

void Audio::Init()
{
    SDL_SetAppMetadata("AUDIO", "1.0", "nothin");
    bool sdlInitSuccess = SDL_Init(SDL_INIT_AUDIO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Audio Tests",
        800,
        600,
        window_flags);

    drwav wav;
    if (!drwav_init_file(&wav, "sounds/254081__capslok__birds_chirping.wav", NULL)) {
        LOG_ERROR("Failed to load wav file");
        exit(1);
    }

    pDecodedInterleavedPCMFrames = static_cast<float*>(malloc(wav.totalPCMFrameCount * wav.channels * sizeof(float)));
    sampleCount = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, pDecodedInterleavedPCMFrames);
    sampleSize = sampleCount * wav.channels * sizeof(float);

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = wav.channels;
    spec.freq = wav.sampleRate;

    // want to define callback to "consume" all items in the audio command buffer
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, this);
    if (!stream) {
        LOG_ERROR("Couldn't create audio stream: {}", SDL_GetError());
        exit(1);
    }

    SDL_ResumeAudioStreamDevice(stream);
    drwav_uninit(&wav);
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
            bShouldExit = true;
            break;
        }

        auto wait = std::chrono::milliseconds(100);
        std::this_thread::sleep_for(wait);
    }
}

void Audio::Cleanup()
{}

} // Audio
