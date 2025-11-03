//
// Created by William on 2025-11-01.
//

#include "resource_manager.h"

#include "render/model/model_loader.h"

namespace Renderer
{
OffsetAllocator::Allocation ResourceManager::AllocateVertices(size_t count)
{
    return vertexBufferAllocator.allocate(count);
}

OffsetAllocator::Allocation ResourceManager::AllocateIndices(size_t count)
{
    return indexBufferAllocator.allocate(count);
}

OffsetAllocator::Allocation ResourceManager::AllocateMaterials(size_t count)
{
    return materialBufferAllocator.allocate(count);
}

OffsetAllocator::Allocation ResourceManager::AllocatePrimitives(size_t count)
{
    return primitiveBufferAllocator.allocate(count);
}
} // Renderer