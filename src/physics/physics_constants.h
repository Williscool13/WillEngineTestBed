//
// Created by William on 2025-10-15.
//

#ifndef WILLENGINETESTBED_PHYSICS_CONSTANTS_H
#define WILLENGINETESTBED_PHYSICS_CONSTANTS_H
#include <cstdint>

namespace Physics
{
inline static constexpr int32_t MAX_PHYSICS_JOBS = 2048;
// Mega Stress Test (6703 bodies, no sleep) shows max used tasks is never greater than 48
inline static constexpr int32_t MAX_PHYSICS_TASKS = 256;
} // Physics

#endif //WILLENGINETESTBED_PHYSICS_CONSTANTS_H