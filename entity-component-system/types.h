//
// Created by William on 2025-10-06.
//

#ifndef WILLENGINETESTBED_TYPES_H
#define WILLENGINETESTBED_TYPES_H

#include <chrono>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <utility>

#include "src/crash-handling/logger.h"

// JPH::BodyID not included, using uint64_t as substitute
typedef uint64_t BodyId;
inline BodyId INVALID_BODY_ID = ~0x0u;

struct Transform
{
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 GetModelMatrix() const
    {
        glm::mat4 model{1.0f};
        model = glm::translate(model, position);
        model *= glm::mat4_cast(rotation);
        model = glm::scale(model, scale);
        return model;
    }
};

struct NameEntityComponent
{
    std::string name;

    explicit NameEntityComponent(std::string n) : name(std::move(n)) {}
};

struct RigidBodyEntityComponent
{
    BodyId bodyId{~0x0u};
    glm::vec3 offset{0.0f};

    glm::vec3 velocity{0, 0, 0};
    glm::vec3 angularVelocity{0, 0, 0};

    [[nodiscard]] bool IsValid() const
    {
        return bodyId != INVALID_BODY_ID;
    }
};

struct BounceEntityComponent
{
    float minY = 0.0f;
    float maxY = 10.0f;
};

struct RenderEntityComponent
{
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float pulseFrequency = 1.0f;
    float pulseTime = 0.0f;
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
