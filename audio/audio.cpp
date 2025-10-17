//
// Created by William on 2025-10-16.
//

#include "audio.h"

#include "audio_utils.h"
#include "crash-handling/logger.h"

#include "dr_libs/dr_wav.h"
#include "input/input.h"

namespace Audio
{
void SDLCALL Audio::AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total)
{

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
    Input::Input input = Input::Input::Get();
    input.Init(window, 800, 600);

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = AUDIO_CHANNELS;
    spec.freq = AUDIO_SAMPLE_RATE;

    enki::TaskSchedulerConfig config;
    config.numTaskThreadsToCreate = enki::GetNumHardwareThreads() - 1;
    LOG_INFO("Scheduler operating with {} threads.", config.numTaskThreadsToCreate + 1);
    scheduler.Initialize(config);

    audioSystem.Initialize(&scheduler);

    gunshot = audioSystem.LoadClip("sounds/402013__eardeer__gunshot__high_4.wav");


    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioSystem::AudioCallback, &audioSystem);
    if (!stream) {
        LOG_ERROR("Couldn't create audio stream: {}", SDL_GetError());
        exit(1);
    }

    SDL_ResumeAudioStreamDevice(stream);
}

void Audio::Update()
{
    Input::Input input = Input::Input::Get();
    SDL_Event e;
    bool exit = false;
    while (true) {
        input.FrameReset();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            input.ProcessEvent(e);
        }

        if (exit || input.IsKeyPressed(Input::Key::ESCAPE)) {
            bShouldExit = true;
            break;
        }

        if (input.IsMousePressed(Input::MouseButton::LMB)) {
            audioSystem.PlaySound(gunshot, VolumeToGain(volume), 1.0f, false);
        }

        if (input.IsKeyPressed(Input::Key::NUM_0)) {
            volume = glm::min(1.0f, volume + 0.1f);
        }
        if (input.IsKeyPressed(Input::Key::NUM_9)) {
            volume = glm::max(0.0f, volume - 0.1f);
        }


        audioSystem.ProcessGameCommands();


        auto wait = std::chrono::milliseconds(10);
        std::this_thread::sleep_for(wait);
    }
}

void Audio::Cleanup()
{
    audioSystem.UnloadClip(gunshot);
    audioSystem.Cleanup();
    scheduler.WaitforAllAndShutdown();
}
} // Audio
