//
// Created by William on 2025-11-06.
//

#include "render_operation_ring_buffer.h"

#include "crash-handling/logger_helpers.h"

namespace Renderer
{
void ModelMatrixOperationRingBuffer::Enqueue(const std::vector<ModelMatrixOperation>& operations)
{
    count += operations.size();
    if (count > capacity) {
        LOG_ERROR("ModelMatrix operation buffer has exceeded count limit.");
    }
    for (const ModelMatrixOperation& op : operations) {
        buffer[head] = op;
        head = (head + 1) % capacity;
    }
}

void InstanceOperationRingBuffer::Enqueue(const std::vector<InstanceOperation>& operations)
{
    count += operations.size();
    if (count > capacity) {
        LOG_ERROR("Instance operation buffer has exceeded count limit.");
    }
    for (const InstanceOperation& op : operations) {
        buffer[head] = op;
        head = (head + 1) % capacity;
    }
}

void JointMatrixOperationRingBuffer::Enqueue(const std::vector<JointMatrixOperation>& operations)
{
    count += operations.size();
    if (count > capacity) {
        LOG_ERROR("JointMatrix operation buffer has exceeded count limit.");
    }
    for (const JointMatrixOperation& op : operations) {
        buffer[head] = op;
        head = (head + 1) % capacity;
    }
}
} // Renderer
