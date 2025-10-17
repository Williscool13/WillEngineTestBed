//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_SYSTEM_H
#define WILLENGINETESTBED_AUDIO_SYSTEM_H
#include <array>
#include <queue>
#include <string>

#include <enkiTS/src/TaskScheduler.h>
#include <LockFreeQueue/LockFreeQueueCpp11.h>
#include <SDL3/SDL.h>

#include "audio_clip.h"
#include "audio_commands.h"
#include "audio_constants.h"
#include "audio_source.h"
#include "audio_types.h"
#include "crash-handling/logger.h"


namespace Audio
{
class AudioSystem
{
public:
    AudioSystem();

    static void SDLCALL AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total);

    void Initialize(enki::TaskScheduler* scheduler_);

    void ProcessGameCommands();

    void Cleanup();

    AudioClipHandle LoadClip(const std::string& path);

    void UnloadClip(AudioClipHandle clipHandle);

    AudioClip* GetClip(AudioClipHandle clipHandle);

    AudioSourceHandle PlaySound(AudioClipHandle clipHandle, float volume, float pitch, bool bLooping)
    {
        return PlaySound(clipHandle, {}, {}, volume, pitch, false, bLooping);
    }

    AudioSourceHandle PlaySound(AudioClipHandle clipHandle, glm::vec3 position, glm::vec3 velocity, float volume, float pitch, bool bSpatial, bool bLooping);

    AudioSource* GetSource(AudioSourceHandle sourceHandle);



    LockFreeQueueCpp11<GameToAudioCommand>& GetAudioCommands() { return gameToAudioCommands; }


private:
    uint32_t AllocateSource();

    void DeallocateSource(AudioSourceHandle sourceHandle);

    uint32_t AllocateClip();

    void DeallocateClip(AudioClipHandle clipHandle);

private:
    std::array<AudioClip, AUDIO_CLIP_LIMIT> clips{};
    std::array<uint32_t, AUDIO_CLIP_LIMIT> clipGenerations{};
    std::queue<uint32_t> freeClipIndices{};
    std::unordered_map<std::string, AudioClipHandle> loadedClips{};

    std::array<AudioSource, AUDIO_SOURCE_LIMIT> audioSources{};
    std::array<uint32_t, AUDIO_CLIP_LIMIT> sourceGenerations{};
    std::queue<uint32_t> freeSourceIndices{};

    std::vector<AudioClipHandle> pendingUnloads{};

    enki::TaskScheduler* scheduler{nullptr};

private: // Audio thread only.
    void ProcessAudioCommands();

    void MixActiveSources(SDL_AudioStream* stream, int bytesNeeded);

    std::vector<AudioSourceHandle> activeSources;

    // Lock free queues that facilitates all communication between game and audio thread.
    LockFreeQueueCpp11<GameToAudioCommand> gameToAudioCommands{AUDIO_COMMAND_LIMIT};
    LockFreeQueueCpp11<AudioToGameCommand> audioToGameCommands{AUDIO_COMMAND_LIMIT};
};
} // Audio

#endif //WILLENGINETESTBED_AUDIO_SYSTEM_H
