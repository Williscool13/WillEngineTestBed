//
// Created by William on 2025-10-15.
//

#ifndef WILLENGINETESTBED_UTILS_H
#define WILLENGINETESTBED_UTILS_H
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
} // Utils

#endif //WILLENGINETESTBED_UTILS_H