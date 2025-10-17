//
// Created by William on 2025-10-15.
//

#ifndef WILLENGINETESTBED_PHYSICS_CONSTANTS_H
#define WILLENGINETESTBED_PHYSICS_CONSTANTS_H
#include <cstdint>

namespace Physics
{
inline static constexpr int32_t MAX_PHYSICS_JOBS = 2048;
inline static constexpr uint64_t TASK_BUFFER = 64;
// Mega Stress Test (6703 bodies, no sleep) shows max used tasks is never greater than 48. But just in case (technical possibility, we make tasks = jobs + buffer.
inline static constexpr int32_t MAX_PHYSICS_TASKS = 2048 + TASK_BUFFER;
} // Physics

#endif //WILLENGINETESTBED_PHYSICS_CONSTANTS_H