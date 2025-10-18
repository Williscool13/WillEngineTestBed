//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_TYPES_H
#define WILLENGINETESTBED_AUDIO_TYPES_H
#include "audio_constants.h"


namespace Audio
{
struct AudioSource;

enum class AudioFormat
{
    WAV,
    MP3,
    OGG,
    Unknown
};

struct AudioClipHandle
{
    uint32_t index{INVALID_CLIP_INDEX};
    uint32_t generation{INVALID_CLIP_INDEX};

    bool IsValid() const { return index != INVALID_CLIP_INDEX && generation != INVALID_CLIP_INDEX; }
};

struct AudioSourceHandle
{
    uint32_t index{INVALID_SOURCE_INDEX};
    uint32_t generation{INVALID_SOURCE_INDEX};

    static AudioSourceHandle INVALID;

    bool IsValid() const { return index != INVALID_SOURCE_INDEX && generation != INVALID_SOURCE_INDEX; }

    bool operator<(const AudioSourceHandle& other) const
    {
        return index < other.index;
    }
};
}


#endif //WILLENGINETESTBED_AUDIO_TYPES_H
