//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_CONSTANTS_H
#define WILLENGINETESTBED_AUDIO_CONSTANTS_H
#include <cstdint>

namespace Audio
{
inline static constexpr uint32_t AUDIO_CHANNELS(2);
inline static constexpr uint32_t AUDIO_SAMPLE_RATE(48000);
inline static constexpr uint32_t AUDIO_SOURCE_LIMIT(64);
inline static constexpr uint32_t AUDIO_CLIP_LIMIT(256);
inline static constexpr uint32_t AUDIO_COMMAND_LIMIT(512);
inline static constexpr uint32_t INVALID_CLIP_INDEX{~0u};
inline static constexpr uint32_t INVALID_SOURCE_INDEX{~0u};
inline static constexpr float SPEED_OF_SOUND{343.0f};
inline static constexpr float MAX_SOUND_DISTANCE{100.0f};
inline static constexpr float MIN_SOUND_DISTANCE{1.0f};

} // Audio

#endif //WILLENGINETESTBED_AUDIO_CONSTANTS_H
