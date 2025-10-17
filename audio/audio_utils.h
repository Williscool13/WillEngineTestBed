//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_UTILS_H
#define WILLENGINETESTBED_AUDIO_UTILS_H
#include <string>

#include "audio_types.h"

namespace Audio
{
AudioFormat GetAudioExtension(const std::string& path);
} // Audio

#endif //WILLENGINETESTBED_AUDIO_UTILS_H