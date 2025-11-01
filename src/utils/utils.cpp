//
// Created by William on 2025-10-15.
//

#include "utils.h"

namespace Utils
{
FrameTimeTracker::FrameTimeTracker(size_t historySize, float spikeThreshold)
    : historySize(std::min(historySize, MAX_HISTORY_SIZE))
      , spikeThreshold(spikeThreshold)
{}

void FrameTimeTracker::RecordFrameTime(float frameTimeMs)
{
    history[currentIndex] = frameTimeMs;
    currentIndex = (currentIndex + 1) % historySize;

    if (sampleCount < historySize) {
        sampleCount++;
    }

    UpdateRollingAverage();
}

float FrameTimeTracker::GetLatestFrameTime() const
{
    if (sampleCount == 0) return 0.0f;

    size_t latestIndex = (currentIndex == 0) ? historySize - 1 : currentIndex - 1;
    return history[latestIndex];
}

void FrameTimeTracker::UpdateRollingAverage()
{
    float sum = 0.0f;
    for (size_t i = 0; i < sampleCount; i++) {
        sum += history[i];
    }
    rollingAverage = sum / static_cast<float>(sampleCount);
}

bool FrameTimeTracker::IsSpikeDetected(float frameTimeMs) const
{
    // Need at least 10 samples to detect spikes reliably
    if (sampleCount < 10) return false;

    return frameTimeMs > rollingAverage * spikeThreshold;
}
} // Utils
