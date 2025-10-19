//
// Created by William on 2025-10-17.
//

#ifndef WILLENGINETESTBED_LOAD_AUDIO_CLIP_TASK_H
#define WILLENGINETESTBED_LOAD_AUDIO_CLIP_TASK_H
#include <string>

#include "audio_types.h"
#include "TaskScheduler.h"

namespace Audio
{
struct AudioClip;

struct LoadAudioClipTask : enki::ITaskSet
{
    AudioClip* clip;
    // path should be same as clip.path, but duplicated here to avoid synchronization requirements
    std::string path;
    AudioFormat format;

    LoadAudioClipTask() = default;
    LoadAudioClipTask(AudioClip* clip_, std::string path_, AudioFormat format_);

    void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override;
};
} // Audio

#endif //WILLENGINETESTBED_LOAD_AUDIO_CLIP_TASK_H