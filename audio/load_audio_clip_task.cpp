//
// Created by William on 2025-10-17.
//

#include "load_audio_clip_task.h"

#include "audio_clip.h"
#include "audio_constants.h"
#include "crash-handling/logger.h"
#include "dr_libs/dr_wav.h"

namespace Audio
{
LoadAudioClipTask::LoadAudioClipTask(AudioClip* clip_, std::string path_, AudioFormat format_)
{
    clip = clip_;
    path = path_;
    format = format_;
}

void LoadAudioClipTask::ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum)
{
    const char* cStrPath = path.c_str();
    if (format == AudioFormat::WAV) {
        drwav wav;
        if (!drwav_init_file(&wav, cStrPath, NULL)) {
            LOG_ERROR("Failed to load wav file {}", cStrPath);
            clip->loadState.store(AudioClip::LoadState::Unloaded, std::memory_order_release);
            return;
        }
        if (wav.channels != AUDIO_CHANNELS || wav.sampleRate != AUDIO_SAMPLE_RATE) {
            LOG_ERROR("Audio file {} has wrong format ({}ch @ {}Hz), expected ({}ch @ {}Hz)", cStrPath, wav.channels, wav.sampleRate, AUDIO_CHANNELS, AUDIO_SAMPLE_RATE);
            drwav_uninit(&wav);
            clip->loadState.store(AudioClip::LoadState::Unloaded, std::memory_order_release);
            return;
        }

        auto pDecodedInterleavedPCMFrames = static_cast<float*>(malloc(wav.totalPCMFrameCount * wav.channels * sizeof(float)));
        clip->sampleCount = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, pDecodedInterleavedPCMFrames);
        clip->data = pDecodedInterleavedPCMFrames;

        drwav_uninit(&wav);
    }

    clip->loadState.store(AudioClip::LoadState::Loaded, std::memory_order_release);
}
} // Audio