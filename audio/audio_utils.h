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
/**
 * Not the cheapest
 * @param volume [0, 1]
 * @return
 */
float VolumeToGain(float volume);
float VolumeToGainCheap(float volume);
} // Audio

#endif //WILLENGINETESTBED_AUDIO_UTILS_H