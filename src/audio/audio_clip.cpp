//
// Created by William on 2025-10-17.
//

#include "audio_clip.h"

namespace Audio
{
AudioClip::AudioClip() : loadTask(std::make_unique<LoadAudioClipTask>()) {};

AudioClip::~AudioClip() = default;

AudioClip::AudioClip(AudioClip&& other) noexcept
    : loadState(other.loadState.load())
    , path(std::move(other.path))
    , data(other.data)
    , sampleCount(other.sampleCount)
    , loadTask(std::move(other.loadTask))
    , handleRefCount(other.handleRefCount)
    , sourceRefCount(other.sourceRefCount)
{
}

AudioClip& AudioClip::operator=(AudioClip&& other) noexcept
{
    if (this != &other)
    {
        loadState.store(other.loadState.load());
        path = std::move(other.path);
        data = other.data;
        sampleCount = other.sampleCount;
        loadTask = std::move(other.loadTask);
        handleRefCount = other.handleRefCount;
        sourceRefCount = other.sourceRefCount;
    }
    return *this;
}
} // Audio
