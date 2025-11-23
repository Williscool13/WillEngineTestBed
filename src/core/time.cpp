//
// Created by William on 2025-10-30.
//

#include "time.h"

namespace Core
{
Time::Time()
{
    lastTime = std::chrono::steady_clock::now();
}

void Time::Reset()
{
    lastTime = std::chrono::steady_clock::now();
}

void Time::Update()
{
    const auto now = std::chrono::steady_clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime);
    deltaTime = delta.count();

    // Breakpoint resume case
    if (deltaTime > 1000) { deltaTime = 333; }
    lastTime = now;
}

float Time::GetDeltaTime() const
{
    return static_cast<float>(deltaTime) / 1000.0f;
}

float Time::GetTime() const
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(lastTime.time_since_epoch());
    return static_cast<float>(elapsed.count()) / 1000.0f;
}
} // Core