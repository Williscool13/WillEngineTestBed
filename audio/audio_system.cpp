// Created by William on 2025-10-17.
//

#include "audio_system.h"

#include "audio_commands.h"
#include "audio_utils.h"
#include "glm/glm.hpp"
#include "utils/utils.h"
#include "utils/world_constants.h"

namespace Audio
{
AudioSystem::AudioSystem()
{
    for (uint32_t i = 0; i < AUDIO_CLIP_LIMIT; ++i) {
        freeClipIndices.push(i);
        clipGenerations[i] = 0;
    }
    for (uint32_t i = 0; i < AUDIO_SOURCE_LIMIT; ++i) {
        freeSourceIndices.push(i);
        sourceGenerations[i] = 0;
    }

    pendingUnloads.reserve(32);
}

void AudioSystem::Initialize(enki::TaskScheduler* scheduler_)
{
    scheduler = scheduler_;
}

void AudioSystem::ProcessGameCommands()
{
    AudioToGameCommand cmd;
    while (audioToGameCommands.pop(cmd)) {
        if (cmd.type == AudioToGameCommandType::Finish) {
            DeallocateSource(cmd.sourceHandle);
        }
    }

    for (auto it = pendingUnloads.begin(); it != pendingUnloads.end();) {
        AudioClip* clip = GetClip(*it);
        if (!clip) {
            LOG_WARN("Audio clipped that was unload delayed found to already be invalid");
            it = pendingUnloads.erase(it);
            continue;
        }

        if (clip->handleRefCount > 0) {
            LOG_INFO("Audio clip that was queued for destruction (waiting for source to finish) was reloaded by another component");
            it = pendingUnloads.erase(it);
            continue;
        }

        if (clip->sourceRefCount > 0) {
            // still a valid destroy, we will keep waiting. (potentially catastrophic infinite wait if the audio clip is looping, also long wait if clip is very long)
            ++it;
            continue;
        }

        if (clip->loadState.load() == AudioClip::LoadState::Loading) {
            ++it;
            continue;
        }

        DeallocateClip(*it);
        it = pendingUnloads.erase(it);
    }
}

void AudioSystem::Cleanup()
{
    if (scheduler) {
        // Wait for all async load tasks to complete
        for (auto& clip : clips) {
            if (clip.loadState.load() == AudioClip::LoadState::Loading) {
                scheduler->WaitforTask(&clip.loadTask);
            }
        }
    }


    // Free all clip data
    for (auto& clip : clips) {
        if (clip.data) {
            free(clip.data);
            clip.data = nullptr;
        }
    }

    loadedClips.clear();
    activeSources.clear();
    pendingUnloads.clear();
}

AudioClipHandle AudioSystem::LoadClip(const std::string& path)
{
    if (!scheduler) {
        LOG_ERROR("Attempted to load clip but schedule is null");
        return {};
    }

    auto it = loadedClips.find(path);
    if (it != loadedClips.end()) {
        AudioClip* clip = GetClip(it->second);
        if (clip) { clip->handleRefCount++; }
        return it->second;
    }

    const uint32_t index = AllocateClip();
    if (index == INVALID_CLIP_INDEX) {
        return {};
    }
    const uint32_t generation = clipGenerations[index];

    AudioClip& clip = clips[index];
    clip.path = path;
    clip.loadState.store(AudioClip::LoadState::Loading);
    clip.loadTask.clip = &clip;
    clip.loadTask.path = path;
    clip.loadTask.format = GetAudioExtension(path);
    clip.handleRefCount = 1;
    scheduler->AddTaskSetToPipe(&clip.loadTask);

    loadedClips[path] = {index, generation};
    return loadedClips[path];
}


AudioClip* AudioSystem::GetClip(AudioClipHandle clipHandle)
{
    if (!clipHandle.IsValid()) return nullptr;

    if (clipGenerations[clipHandle.index] != clipHandle.generation) {
        return nullptr;
    }

    AudioClip& clip = clips[clipHandle.index];
    if (clip.loadState.load() != AudioClip::LoadState::Loaded) {
        return nullptr;
    }

    return &clip;
}

AudioSourceHandle AudioSystem::PlaySound(AudioClipHandle clipHandle, glm::vec3 position, glm::vec3 velocity, float volume, float pitch, bool bSpatial, bool bDoppler, bool bLooping)
{
    AudioClip* clip = GetClip(clipHandle);
    if (!clip) { return AudioSourceHandle::INVALID; }

    clip->sourceRefCount++;

    uint32_t index = AllocateSource();
    if (index == INVALID_CLIP_INDEX) {
        return {};
    }
    const uint32_t generation = sourceGenerations[index];
    // todo properties from the component that calls
    AudioSource& source = audioSources[index];
    source.clip = clip;
    source.position.store(position);
    source.velocity.store(velocity);
    source.baseVolume = volume;
    source.basePitch = glm::clamp(pitch, 0.05f, 10.0f);
    source.looping = bLooping;
    source.spatial = bSpatial;
    source.doppler = bDoppler;
    source.dopplerPitch = 1.0f;//CalculateDopplerShift(position, velocity, listenerPosition, listenerVelocity);

    AudioSourceHandle sourceHandle = {index, generation};
    bool res = gameToAudioCommands.push({GameToAudioCommandType::Play, sourceHandle});
    if (!res) {
        LOG_ERROR("[AudioSystem::PlaySound] Audio to Game command addition failed. Command terminated.");
        DeallocateSource(sourceHandle);
        return {};
    }

    return sourceHandle;
}

bool AudioSystem::StopSound(AudioSourceHandle sourceHandle)
{
    bool res = gameToAudioCommands.push({GameToAudioCommandType::Stop, sourceHandle});
    if (!res) {
        LOG_ERROR("[AudioSystem::StopSound] Audio to Game command addition failed.");
        return {};
    }
    return res;
}

AudioSource* AudioSystem::GetSource(AudioSourceHandle sourceHandle)
{
    if (!sourceHandle.IsValid()) return nullptr;

    if (sourceGenerations[sourceHandle.index] != sourceHandle.generation) {
        return nullptr;
    }

    return &audioSources[sourceHandle.index];
}

void AudioSystem::UnloadClip(AudioClipHandle clipHandle)
{
    AudioClip* clip = GetClip(clipHandle);
    if (!clip) return;

    clip->handleRefCount--;

    if (clip->handleRefCount > 0) {
        return;
    }

    if (clip->sourceRefCount > 0) {
        pendingUnloads.push_back(clipHandle);
        return;
    }

    if (clip->loadState.load() == AudioClip::LoadState::Loading) {
        pendingUnloads.push_back(clipHandle);
        return;
    }

    DeallocateClip(clipHandle);
}

bool AudioSystem::IsAudioSourceValid(AudioSourceHandle sourceHandle) const
{
    if (sourceHandle.index >= AUDIO_SOURCE_LIMIT) { return false; }
    if (sourceGenerations[sourceHandle.index] != sourceHandle.generation) { return false; }
    return true;
}

uint32_t AudioSystem::AllocateSource()
{
    if (freeSourceIndices.empty()) {
        LOG_ERROR("Audio source limit reached!");
        return INVALID_SOURCE_INDEX;
    }

    uint32_t index = freeSourceIndices.front();
    freeSourceIndices.pop();
    return index;
}

void AudioSystem::DeallocateSource(AudioSourceHandle sourceHandle)
{
    if (sourceHandle.index >= AUDIO_SOURCE_LIMIT) { return; }
    if (sourceGenerations[sourceHandle.index] != sourceHandle.generation) { return; }

    sourceGenerations[sourceHandle.index]++;

    AudioSource& source = audioSources[sourceHandle.index];
    if (source.clip) {
        source.clip->sourceRefCount--;
    }

    source.clip = nullptr;
    source.playbackPosition = 0;
    source.bIsPlaying = false;
    source.bIsFinished = false;
    source.position.store(glm::vec3(0.0f));
    source.velocity.store(glm::vec3(0.0f));

    freeSourceIndices.push(sourceHandle.index);
}

uint32_t AudioSystem::AllocateClip()
{
    if (freeClipIndices.empty()) {
        LOG_ERROR("Audio clip limit reached!");
        return INVALID_CLIP_INDEX;
    }

    uint32_t index = freeClipIndices.front();
    freeClipIndices.pop();
    return index;
}

void AudioSystem::DeallocateClip(AudioClipHandle clipHandle)
{
    if (clipHandle.index >= AUDIO_CLIP_LIMIT) { return; }
    if (clipGenerations[clipHandle.index] != clipHandle.generation) { return; }

    clipGenerations[clipHandle.index]++;

    AudioClip& clip = clips[clipHandle.index];
    if (clip.data) {
        free(clip.data);
        clip.data = nullptr;
    }

    clip.path.clear();
    clip.sampleCount = 0;
    clip.loadTask.clip = nullptr;
    clip.loadTask.path.clear();
    clip.loadTask.format = AudioFormat::Unknown;

    assert(clip.handleRefCount == 0);
    assert(clip.sourceRefCount == 0);


    clip.loadState.store(AudioClip::LoadState::Unloaded);
    loadedClips.erase(clip.path);
    freeClipIndices.push(clipHandle.index);
}

void AudioSystem::AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total)
{
    auto* self = static_cast<AudioSystem*>(userdata);
    self->ProcessAudioCommands();
    std::sort(self->activeSources.begin(), self->activeSources.end());
    self->MixActiveSources(stream, additional);
}


void AudioSystem::ProcessAudioCommands()
{
    // Check for finished sources
    for (auto it = activeSources.begin(); it != activeSources.end();) {
        AudioSource* source = GetSource(*it);
        if (!source) {
            LOG_WARN("Active source handle is invalid");
            it = activeSources.erase(it);
            continue;
        }

        if (source->bIsFinished || !source->clip) {
            bool res = audioToGameCommands.push({AudioToGameCommandType::Finish, *it});
            if (!res) {
                LOG_WARN("Audio to Game command addition failed, trying again next frame");
                continue;
            }
            it = activeSources.erase(it);
        }
        else {
            ++it;
        }
    }

    // Add all play commands to active sources.
    GameToAudioCommand cmd;
    while (gameToAudioCommands.pop(cmd)) {
        if (cmd.type == GameToAudioCommandType::Play) {
            AudioSource* source = GetSource(cmd.sourceHandle);
            if (source) {
                source->bIsPlaying = true;
                activeSources.push_back(cmd.sourceHandle);
            }
        }
        else if (cmd.type == GameToAudioCommandType::Stop) {
            AudioSource* source = GetSource(cmd.sourceHandle);
            if (source) {
                source->bIsFinished = true;
            }
        }
    }
}

void AudioSystem::MixActiveSources(SDL_AudioStream* stream, int bytesNeeded)
{
    if (bytesNeeded <= 0) return;

    const int samplesNeeded = bytesNeeded / sizeof(float);
    std::vector mixBuffer(samplesNeeded, 0.0f);

    // For each source, add audio clip's sample contribution to the sample buffer
    {
        //Utils::ScopedTimer scopedTimer{"Sound Mixing"};
        float distanceAttenuation;
        float panRight;
        float panMuffle = 1.0f;

        for (auto& sourceHandle : activeSources) {
            AudioSource* source = GetSource(sourceHandle);
            if (!source || !source->bIsPlaying || source->bIsFinished) { continue; }


            if (source->spatial) {
                glm::vec3 sourcePos = source->position.load(std::memory_order_relaxed);
                glm::vec3 sourceVel = source->velocity.load(std::memory_order_relaxed);
                glm::vec3 listenerPos = listenerPosition.load(std::memory_order_relaxed);
                glm::vec3 listenerVel = listenerVelocity.load(std::memory_order_relaxed);


                float distance = glm::distance(sourcePos, listenerPos);

                // Distance Limit
                {
                    // Too far, but continue to play in case player gets closer later
                    if (distance > MAX_SOUND_DISTANCE) {
                        // need to add the loop/finish here too
                        source->playbackPosition += source->basePitch * samplesNeeded;
                        if (!source->looping && source->playbackPosition >= source->clip->sampleCount) {
                            if (source->looping) {
                                source->playbackPosition = 0;
                            }
                            else {
                                source->bIsFinished = true;
                                source->bIsPlaying = false;
                            }
                        }
                        continue;
                    }
                }

                distance = glm::clamp(distance, MIN_SOUND_DISTANCE, MAX_SOUND_DISTANCE);
                distanceAttenuation = MIN_SOUND_DISTANCE / distance;

                glm::vec3 listenerFwd = listenerForward.load(std::memory_order_relaxed);
                glm::vec3 listenerRight = glm::cross(listenerFwd, WorldConstants::WORLD_UP);
                glm::vec3 toSource = glm::normalize(sourcePos - listenerPos);
                panRight = glm::dot(toSource, listenerRight);

                // Sound is coming from behind.
                // Not much we can do with stereo, will just muffle sound to make "behind" distinction.
                if (glm::dot(toSource, listenerFwd) < 0.0f) {
                    panMuffle = 0.75f;
                }

                if (source->doppler) {
                    auto target = CalculateDopplerShift(sourcePos, sourceVel, listenerPos, listenerVel);
                    source->dopplerPitch = glm::mix(source->dopplerPitch, target, 0.1f);
                }
            }
            else {
                distanceAttenuation = 1.0f;
                panRight = 0.0f;
                panMuffle = 1.0f;
            }

            for (int i = 0; i < samplesNeeded; ++i) {
                const uint64_t samplePos = std::floor(source->playbackPosition);

                float fraction = source->playbackPosition - samplePos;
                uint64_t nextSamplePos = samplePos + 1;

                float sample = source->clip->data[samplePos];
                if (nextSamplePos < source->clip->sampleCount) {
                    sample = glm::mix(sample, source->clip->data[nextSamplePos], fraction);
                }
                else {
                    // About to hit the end.
                    if (source->looping) {
                        nextSamplePos %= source->clip->sampleCount;
                        sample = glm::mix(sample, source->clip->data[nextSamplePos], fraction);
                        source->playbackPosition = 0;
                    }
                }

                // stereo is LRLRLR
                // 0 is left,  so - panRight
                // 1 is right, so + panRight
                float pan = (samplePos & 1) == 0 ? 1.0f - panRight : 1.0f + panRight;
                mixBuffer[i] += sample * source->baseVolume * distanceAttenuation * pan * panMuffle;
                source->playbackPosition += source->basePitch * source->dopplerPitch;

                if (!source->looping && source->playbackPosition >= source->clip->sampleCount) {
                    source->bIsFinished = true;
                    source->bIsPlaying = false;
                    break;
                }
            }
        }
    }


    SDL_PutAudioStreamData(stream, mixBuffer.data(), bytesNeeded);
}
} // Audio
