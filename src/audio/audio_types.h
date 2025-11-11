//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_TYPES_H
#define WILLENGINETESTBED_AUDIO_TYPES_H

#include "core/data-structures/free_list.h"


namespace Audio
{
struct AudioClip;
struct AudioSource;

enum class AudioFormat
{
    WAV,
    MP3,
    OGG,
    Unknown
};

using AudioClipHandle = Handle<AudioClip>;
using AudioSourceHandle = Handle<AudioSource>;
}


#endif //WILLENGINETESTBED_AUDIO_TYPES_H
