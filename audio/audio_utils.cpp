//
// Created by William on 2025-10-17.
//

#include "audio_utils.h"

#include <filesystem>

#include "glm/glm.hpp"

namespace Audio
{
AudioFormat GetAudioExtension(const std::string& path)
{
    auto ext = std::filesystem::path(path).extension().string();

    // Convert to lowercase for case-insensitive comparison
    std::ranges::transform(ext, ext.begin(), ::tolower);

    if (ext == ".wav") return AudioFormat::WAV;
    if (ext == ".mp3") return AudioFormat::MP3;
    if (ext == ".ogg") return AudioFormat::OGG;

    return AudioFormat::Unknown;
}

float VolumeToGain(float volume)
{
    if (volume <= 0.0f) return 0.0f;

    // Map 0-100% to -60dB to 0dB
    const float dB = -60.0f + (volume * 60.0f);
    return glm::pow(10.0f, dB / 20.0f);
}

float VolumeToGainCheap(float volume)
{
    return glm::pow(volume, 2);
}
} // Audio
