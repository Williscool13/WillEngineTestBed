//
// Created by William on 2025-10-16.
//

#include "audio.h"

#include "../src/audio/audio_utils.h"
#include "crash-handling/logger.h"

#include "dr_libs/dr_wav.h"
#include "input/input.h"

namespace Audio
{
void SDLCALL Audio::AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total)
{}

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
    whistle = audioSystem.LoadClip("sounds/464885__godoy__postmens-whistle_mod.wav");


    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioSystem::AudioCallback, &audioSystem);
    if (!stream) {
        LOG_ERROR("Couldn't create audio stream: {}", SDL_GetError());
        exit(1);
    }

    SDL_ResumeAudioStreamDevice(stream);
}

void Audio::Update()
{
    glm::vec3 playerPos = glm::vec3(0.0f);
    glm::vec3 playerForward = WORLD_FORWARD;
    glm::vec3 playerRight = glm::cross(playerForward, WORLD_UP);

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

        if (input.IsKeyPressed(Input::Key::NUM_1)) {
            audioSystem.PlaySound(whistle, playerPos + playerForward * 2.0f, glm::vec3(0.0f), 1.0f, pitch, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_2)) {
            audioSystem.PlaySound(gunshot, playerPos - playerForward * 2.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_3)) {
            audioSystem.PlaySound(gunshot, playerPos + playerRight * 2.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_4)) {
            audioSystem.PlaySound(gunshot, playerPos - playerRight * 2.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_5)) {
            audioSystem.PlaySound(gunshot, playerPos + (playerForward + playerRight) * 2.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_6)) {
            audioSystem.PlaySound(gunshot, playerPos + (playerForward - playerRight) * 2.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_7)) {
            audioSystem.PlaySound(gunshot, playerPos + (-playerForward + playerRight) * 2.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_8)) {
            audioSystem.PlaySound(gunshot, playerPos + (-playerForward - playerRight) * 2.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }
        if (input.IsKeyPressed(Input::Key::NUM_9)) {
            audioSystem.PlaySound(gunshot, playerPos + playerForward * 95.0f, glm::vec3(0.0f), 1.0f, 1.0f, true, false, false);
        }

        if (input.IsKeyPressed(Input::Key::NUM_0)) {
            pitch = glm::min(2.0f, pitch + 0.05f);
        }
        if (input.IsKeyPressed(Input::Key::NUM_9)) {
            pitch = glm::max(0.0f, pitch - 0.05f);
        }


        if (input.IsKeyPressed(Input::Key::P)) {
            auto thread = std::jthread(&Audio::TestDopplerEffect, this);
            thread.join();
        }



        audioSystem.ProcessGameCommands();


        auto wait = std::chrono::milliseconds(10);
        std::this_thread::sleep_for(wait);
    }
}

void Audio::Cleanup()
{
    audioSystem.UnloadClip(gunshot);
    audioSystem.UnloadClip(whistle);
    audioSystem.Cleanup();
    scheduler.WaitforAllAndShutdown();
}

void Audio::TestDopplerEffect()
{
    glm::vec3 sourcePos = glm::vec3(1.0f, 0.0f, 15.0f);
    glm::vec3 sourceVel = glm::vec3(0.0f, 0.0f, -5.0f);
    glm::vec3 listenerPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 listenerVel = glm::vec3(0.0f, 0.0f, 0.0f);
    AudioSourceHandle sourceHandle = audioSystem.PlaySound(whistle, sourcePos, sourceVel, 0.03f, 1.0f, true, false, true);

    int32_t count{0};
    while (true) {
        listenerPos += listenerVel * (33.0f / 1000.0f);
        audioSystem.UpdateListener(listenerPos, listenerVel, -WORLD_FORWARD);

        // Update source position/velocity
        if (!audioSystem.IsAudioSourceValid(sourceHandle)) {
            break;
        }

        AudioSource* source = audioSystem.GetSource(sourceHandle);
        // Update source position based on its velocity
        source->position.store(source->position.load() + source->velocity.load() * (33.0f / 1000.0f), std::memory_order_relaxed);

        // Optional: Sleep or sync with frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
        count++;
        if (count >= 150) {
            audioSystem.StopSound(sourceHandle);
            break;
        }
    }
}
} // Audio
