//
// Created by William on 2025-10-30.
//

#include "time.h"

#include <SDL3/SDL.h>

namespace Core
{
Time::Time()
{
    lastTime = SDL_GetTicks();
}

void Time::Reset()
{
    lastTime = SDL_GetTicks();
}

void Time::Update()
{
    const uint64_t last = SDL_GetTicks();
    deltaTime = last - lastTime;
    // Breakpoint resume case
    if (deltaTime > 1000) { deltaTime = 333; }
    lastTime = last;

}

float Time::GetDeltaTime() const
{
    return static_cast<float>(deltaTime) / 1000.0f;
}

float Time::GetTime() const
{
    return static_cast<float>(lastTime) / 1000.0f;
}
} // Core