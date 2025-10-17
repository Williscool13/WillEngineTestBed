//
// Created by William on 2025-10-17.
//

#include "audio_utils.h"

#include <filesystem>

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
} // Audio
