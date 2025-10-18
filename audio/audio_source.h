//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_SOURCE_H
#define WILLENGINETESTBED_AUDIO_SOURCE_H
#include <atomic>
#include <cstdint>

#include "glm/glm.hpp"

namespace Audio
{
struct AudioClip;

struct AudioSource
{
    // Set by game thread (synchronized)
    alignas(64) std::atomic<glm::vec3> position;
    alignas(64) std::atomic<glm::vec3> velocity;

    // Set once by game thread (no sync)
    AudioClip* clip;
    float baseVolume{1.0f};
    float basePitch{1.0f};
    float baseSpeed{1.0f};
    bool looping{false};
    bool spatial{false};

    // Audio thread only (no sync)
    float playbackPosition{0}; // float because of pitch, which directly affects
    bool bIsPlaying{false};

    bool bIsFinished{false};
};
} // Audio

#endif //WILLENGINETESTBED_AUDIO_SOURCE_H
