//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_SYSTEM_H
#define WILLENGINETESTBED_AUDIO_SYSTEM_H
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
#include "utils/free_list.h"
#include "utils/world_constants.h"


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

    AudioSourceHandle PlaySound(AudioClipHandle clipHandle, float volume, float pitch, bool bLooping)
    {
        return PlaySound(clipHandle, {}, {}, volume, pitch, false, false, bLooping);
    }

    AudioSourceHandle PlaySound(AudioClipHandle clipHandle, glm::vec3 position, glm::vec3 velocity, float volume, float pitch, bool bSpatial, bool bDoppler, bool bLooping);

    bool StopSound(AudioSourceHandle sourceHandle);

    AudioSource* GetSource(AudioSourceHandle sourceHandle);

    LockFreeQueueCpp11<GameToAudioCommand>& GetAudioCommands() { return gameToAudioCommands; }

    void UpdateListener(glm::vec3 position, glm::vec3 velocity, glm::vec3 forward) {
        listenerPosition.store(position);
        listenerVelocity.store(velocity);
        listenerForward.store(forward);
    }

private:
    void DeallocateSource(AudioSourceHandle sourceHandle);

    void DeallocateClip(AudioClipHandle clipHandle);

private:
    enki::TaskScheduler* scheduler{nullptr};

    FreeList<AudioClip, AUDIO_CLIP_LIMIT> clipFreeList{};
    std::unordered_map<std::string, AudioClipHandle> loadedClips{};
    FreeList<AudioSource, AUDIO_CLIP_LIMIT> sourceFreeList{};

    std::vector<AudioClipHandle> pendingUnloads{};

    std::atomic<glm::vec3> listenerPosition{glm::vec3(0.0f)};
    std::atomic<glm::vec3> listenerVelocity{glm::vec3(0.0f)};
    std::atomic<glm::vec3> listenerForward{WORLD_FORWARD};

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
