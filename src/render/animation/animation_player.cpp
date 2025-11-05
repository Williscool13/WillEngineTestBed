//
// Created by William on 2025-10-31.
//

#include "animation_player.h"

namespace Renderer
{
void AnimationPlayer::Play(const Animation& animation, bool loop)
{
    currentAnimation = &animation;
    currentTime = 0.0f;
    isLooping = loop;
    isPlaying = true;
}

void AnimationPlayer::Update(float deltaTime, std::vector<RuntimeNode>& nodes, const std::vector<uint32_t>& nodeRemap)
{
    if (!isPlaying || !currentAnimation) return;

    currentTime += deltaTime;

    if (isLooping && currentTime > currentAnimation->duration) {
        currentTime = std::fmod(currentTime, currentAnimation->duration);
    }
    else if (currentTime > currentAnimation->duration) {
        isPlaying = false;
        return;
    }

    for (const auto& channel : currentAnimation->channels) {
        uint32_t remappedTargetNodeIndex = nodeRemap[channel.targetNodeIndex];
        const auto& sampler = currentAnimation->samplers[channel.samplerIndex];

        size_t nextIndex = 0;
        for (size_t i = 0; i < sampler.timestamps.size(); ++i) {
            if (sampler.timestamps[i] > currentTime) {
                nextIndex = i;
                break;
            }
        }

        if (nextIndex == 0) {
            switch (channel.targetPath) {
                case AnimationChannel::TargetPath::Translation:
                    nodes[remappedTargetNodeIndex].transform.translation = SampleVec3(sampler, nextIndex);
                    continue;
                case AnimationChannel::TargetPath::Rotation:
                    nodes[remappedTargetNodeIndex].transform.rotation = SampleQuat(sampler, nextIndex);
                    continue;
                case AnimationChannel::TargetPath::Scale:
                    nodes[remappedTargetNodeIndex].transform.scale = SampleVec3(sampler, nextIndex);
                    continue;
                case AnimationChannel::TargetPath::Weights:
                    LOG_ERROR("This renderer does not support runtime node weights at this time.");
                    continue;
            }
        }

        const size_t prevIndex = nextIndex - 1;
        const float t0 = sampler.timestamps[prevIndex];
        const float t1 = sampler.timestamps[nextIndex];
        const float factor = (currentTime - t0) / (t1 - t0);

        switch (channel.targetPath) {
            case AnimationChannel::TargetPath::Translation:
                nodes[remappedTargetNodeIndex].transform.translation = SampleVec3(sampler, prevIndex, nextIndex, factor);
                break;
            case AnimationChannel::TargetPath::Rotation:
                nodes[remappedTargetNodeIndex].transform.rotation = SampleQuat(sampler, prevIndex, nextIndex, factor);
                break;
            case AnimationChannel::TargetPath::Scale:
                nodes[remappedTargetNodeIndex].transform.scale = SampleVec3(sampler, prevIndex, nextIndex, factor);
                break;
            case AnimationChannel::TargetPath::Weights:
                LOG_ERROR("This renderer does not support runtime node weights at this time.");
                break;
        }
    }
}

glm::vec3 AnimationPlayer::SampleVec3(const AnimationSampler& sampler, size_t index)
{
    return {
        sampler.values[index * 3 + 0],
        sampler.values[index * 3 + 1],
        sampler.values[index * 3 + 2]
    };
}

glm::vec3 AnimationPlayer::SampleVec3(const AnimationSampler& sampler, size_t prevIndex, size_t nextIndex, float factor)
{
    glm::vec3 v0(
            sampler.values[prevIndex * 3 + 0],
            sampler.values[prevIndex * 3 + 1],
            sampler.values[prevIndex * 3 + 2]
        );
    glm::vec3 v1(
        sampler.values[nextIndex * 3 + 0],
        sampler.values[nextIndex * 3 + 1],
        sampler.values[nextIndex * 3 + 2]
    );

    switch (sampler.interpolation) {
        case AnimationSampler::Interpolation::Linear:
            return glm::mix(v0, v1, factor);
        case AnimationSampler::Interpolation::Step:
            return v0;
        case AnimationSampler::Interpolation::CubicSpline: // todo: figure out what this is
        default:
            return glm::mix(v0, v1, factor);
    }
}

glm::quat AnimationPlayer::SampleQuat(const AnimationSampler& sampler, size_t index)
{
    // glm stores w in 0
    return {
        sampler.values[index * 4 + 3],
        sampler.values[index * 4 + 0],
        sampler.values[index * 4 + 1],
        sampler.values[index * 4 + 2],

    };
}

glm::quat AnimationPlayer::SampleQuat(const AnimationSampler& sampler, size_t prevIndex, size_t nextIndex, float factor)
{
    // glm stores w in 0
    glm::quat v0{
        sampler.values[prevIndex * 4 + 3],
        sampler.values[prevIndex * 4 + 0],
        sampler.values[prevIndex * 4 + 1],
        sampler.values[prevIndex * 4 + 2],

    };
    glm::quat v1{
        sampler.values[nextIndex * 4 + 3],
        sampler.values[nextIndex * 4 + 0],
        sampler.values[nextIndex * 4 + 1],
        sampler.values[nextIndex * 4 + 2],
    };

    switch (sampler.interpolation) {
        case AnimationSampler::Interpolation::Linear:
            return glm::slerp(v0, v1, factor);
        case AnimationSampler::Interpolation::Step:
            return v0;
        case AnimationSampler::Interpolation::CubicSpline: // todo: figure out what this is
        default:
            return glm::slerp(v0, v1, factor);
    }
}
} // Renderer
