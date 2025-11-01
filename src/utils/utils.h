//
// Created by William on 2025-10-15.
//

#ifndef WILLENGINETESTBED_UTILS_H
#define WILLENGINETESTBED_UTILS_H
#include <array>
#include <windows.h>

#include <chrono>
#include <string>

#include "crash-handling/logger.h"

namespace Utils
{
struct ScopedTimer
{
public:
    explicit ScopedTimer(std::string name)
        : name(std::move(name)), start(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        LOG_INFO("{}: {} us ({:.3f} ms)", name, duration.count(), duration.count() / 1000.0);
    }

private:
    std::string name;
    std::chrono::high_resolution_clock::time_point start;
};

inline void SetThreadName(const char* name) {
    // Wide string conversion
    wchar_t wideName[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wideName, 256);
    SetThreadDescription(GetCurrentThread(), wideName);
}

class FrameTimeTracker
{
public:
    explicit FrameTimeTracker(size_t historySize = 100, float spikeThreshold = 1.5f);

    void RecordFrameTime(float frameTimeMs);

    float GetRollingAverage() const { return rollingAverage; }
    float GetLatestFrameTime() const;
    size_t GetSampleCount() const { return sampleCount; }

    void SetSpikeThreshold(float threshold) { spikeThreshold = threshold; }
    float GetSpikeThreshold() const { return spikeThreshold; }

private:
    static constexpr size_t MAX_HISTORY_SIZE = 1000;

    std::array<float, MAX_HISTORY_SIZE> history{};
    size_t historySize;
    size_t currentIndex{0};
    size_t sampleCount{0};
    float rollingAverage{0.0f};
    float spikeThreshold;

    void UpdateRollingAverage();
    bool IsSpikeDetected(float frameTimeMs) const;
};

} // Utils

#endif //WILLENGINETESTBED_UTILS_H