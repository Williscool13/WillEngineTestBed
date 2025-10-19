//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_AUDIO_COMMANDS_H
#define WILLENGINETESTBED_AUDIO_COMMANDS_H

#include "audio_types.h"

namespace Audio
{
enum class GameToAudioCommandType
{
    Play,
    Stop
};

enum class AudioToGameCommandType
{
    Finish
};

struct GameToAudioCommand
{
    GameToAudioCommandType type;
    AudioSourceHandle sourceHandle;
};

struct AudioToGameCommand
{
    AudioToGameCommandType type;
    AudioSourceHandle sourceHandle;
};
} // Audio

#endif //WILLENGINETESTBED_AUDIO_COMMANDS_H
