//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_CLIP_H
#define WILLENGINETESTBED_AUDIO_CLIP_H

#include <filesystem>

#include "load_audio_clip_task.h"
#include "TaskScheduler.h"

namespace Audio
{
struct AudioClip
{
    enum class LoadState : uint8_t { Unloaded, Loading, Loaded };

    std::atomic<LoadState> loadState{LoadState::Unloaded};

    // Modified when allocated (constant)
    std::string path{};
    // Modified by load-code (constants)
    float* data = nullptr;
    size_t sampleCount{0};

    LoadAudioClipTask loadTask;

    // Game thread only.
    uint32_t handleRefCount{0};
    uint32_t sourceRefCount{0};
};
} // Audio

#endif //WILLENGINETESTBED_AUDIO_CLIP_H
