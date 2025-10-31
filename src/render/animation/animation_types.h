//
// Created by William on 2025-10-31.
//

#ifndef WILLENGINETESTBED_ANIMATION_TYPES_H
#define WILLENGINETESTBED_ANIMATION_TYPES_H
#include <string>
#include <vector>

namespace Renderer
{
struct AnimationSampler
{
    enum class Interpolation
    {
        Linear,
        Step,
        CubicSpline,
    };

    std::vector<float> timestamps;
    std::vector<float> values;
    Interpolation interpolation;
};

struct AnimationChannel
{
    enum class TargetPath
    {
        Translation,
        Rotation,
        Scale,
        Weights,
    };

    uint32_t samplerIndex;
    uint32_t targetNodeIndex;
    TargetPath targetPath;
};

struct Animation
{
    std::string name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
    float duration;
};
} // Renderer

#endif //WILLENGINETESTBED_ANIMATION_TYPES_H
