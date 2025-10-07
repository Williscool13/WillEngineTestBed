//
// Created by William on 2025-10-07.
//

#ifndef WILLENGINETESTBED_TYPES_H
#define WILLENGINETESTBED_TYPES_H
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#include "src/crash-handling/logger.h"

struct FrameData
{
    int64_t frameCount{-1};
    std::chrono::high_resolution_clock::time_point gameReleaseTime{};
    std::chrono::high_resolution_clock::time_point renderReleaseTime{};
};

class ScopedTimer
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

#endif //WILLENGINETESTBED_TYPES_H
