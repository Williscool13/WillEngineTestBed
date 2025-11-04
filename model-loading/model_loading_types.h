//
// Created by William on 2025-11-04.
//

#ifndef WILLENGINETESTBED_MODEL_LOADING_TYPES_H
#define WILLENGINETESTBED_MODEL_LOADING_TYPES_H

#include <stdint.h>
#include <glm/glm.hpp>

#include "render/model/model_data.h"

namespace Renderer
{
struct RuntimeMesh
{
    ModelDataHandle modelDataHandle{ModelDataHandle::Invalid};
    // sorted when generated
    std::vector<RuntimeNode> nodes;

    std::vector<uint32_t> nodeRemap{};

    Transform transform;
    OffsetAllocator::Allocation jointMatrixAllocation{};
    uint32_t jointMatrixOffset{0};
};
} // Renderer

#endif //WILLENGINETESTBED_MODEL_LOADING_TYPES_H