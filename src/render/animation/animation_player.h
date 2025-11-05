//
// Created by William on 2025-10-31.
//

#ifndef WILLENGINETESTBED_ANIMATION_PLAYER_H
#define WILLENGINETESTBED_ANIMATION_PLAYER_H
#include <glm/glm.hpp>

#include "animation_types.h"
#include "crash-handling/logger.h"
#include "render/model/model_data.h"

namespace Renderer
{
class AnimationPlayer
{
public:
    AnimationPlayer() = default;

public:
    void Play(const Animation& animation, bool loop = true);

    void Update(float deltaTime, std::vector<RuntimeNode>& nodes, const std::vector<uint32_t>& nodeRemap);

private:
    static glm::vec3 SampleVec3(const AnimationSampler& sampler, size_t index);

    static glm::vec3 SampleVec3(const AnimationSampler& sampler, size_t prevIndex, size_t nextIndex, float factor);

    static glm::quat SampleQuat(const AnimationSampler& sampler, size_t index);

    static glm::quat SampleQuat(const AnimationSampler& sampler, size_t prevIndex, size_t nextIndex, float factor);

private:
    const Animation* currentAnimation = nullptr;
    float currentTime = 0.0f;
    bool isPlaying = false;
    bool isLooping = false;
};
} // Renderer

#endif //WILLENGINETESTBED_ANIMATION_PLAYER_H
