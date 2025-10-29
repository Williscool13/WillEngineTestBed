//
// Created by William on 2025-10-29.
//

#ifndef WILLENGINETESTBED_TRANSFORM_H
#define WILLENGINETESTBED_TRANSFORM_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Core
{
struct Transform
{
    glm::vec3 translation{};
    glm::quat rotation{};
    glm::vec3 scale{};

    [[nodiscard]] glm::mat4 GetMatrix() const { return glm::translate(glm::mat4(1.0f), translation) * mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale); }

    static const Transform Identity;
};

inline const Transform Transform::Identity{
    {0.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 1.0f}
};
} // core

using Core::Transform;

#endif //WILLENGINETESTBED_TRANSFORM_H
