//
// Created by William on 2025-11-06.
//

#ifndef WILLENGINETESTBED_RENDER_OPERATION_RING_BUFFER_H
#define WILLENGINETESTBED_RENDER_OPERATION_RING_BUFFER_H

#include <stdint.h>
#include <vector>

#include "render_operations.h"
#include "crash-handling/logger.h"

namespace Renderer
{
class ModelMatrixOperationRingBuffer
{
public:
    void Initialize(size_t capacity_)
    {
        buffer.resize(capacity_);
        head = 0;
        tail = 0;
        count = 0;
        capacity = capacity_;
    }

    void Enqueue(const std::vector<ModelMatrixOperation>& operations)
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

    void ProcessOperations(char* pMappedData, uint32_t discardCount)
    {
        for (size_t i = 0; i < count; ++i) {
            const uint32_t opIndex = (tail + i) % capacity;
            ModelMatrixOperation& op = buffer[opIndex];

            if (op.frames == 0) {
                memcpy(pMappedData + op.index * sizeof(Model) + offsetof(Model, modelMatrix),
                       &op.modelMatrix, sizeof(glm::mat4));
            }
            else {
                memcpy(pMappedData + op.index * sizeof(Model) + offsetof(Model, prevModelMatrix), &op.modelMatrix, sizeof(glm::mat4));
                memcpy(pMappedData + op.index * sizeof(Model) + offsetof(Model, modelMatrix), &op.modelMatrix, sizeof(glm::mat4));
            }

            op.frames++;
            if (op.frames == discardCount) {
                tail = (tail + 1) % capacity;
                count--;
            }
        }
    }

private:
    std::vector<ModelMatrixOperation> buffer;
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    size_t capacity = 0;
};

class InstanceOperationRingBuffer
{
public:
    void Initialize(size_t capacity_)
    {
        buffer.resize(capacity_);
        head = 0;
        tail = 0;
        count = 0;
        capacity = capacity_;
    }

    void Enqueue(const std::vector<InstanceOperation>& operations)
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

    void ProcessOperations(char* pMappedData, uint32_t discardCount, uint32_t& highestInstanceIndex)
    {
        for (size_t i = 0; i < count; ++i) {
            const uint32_t opIndex = (tail + i) % capacity;
            InstanceOperation& op = buffer[opIndex];

            memcpy(pMappedData + sizeof(Instance) * op.index, &op.instance, sizeof(Instance));
            highestInstanceIndex = glm::max(highestInstanceIndex, op.index + 1);

            op.frames++;
            if (op.frames == discardCount) {
                tail = (tail + 1) % capacity;
                count--;
            }
        }
    }

private:
    std::vector<InstanceOperation> buffer;
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    size_t capacity = 0;
};

class JointMatrixOperationRingBuffer
{
public:
    void Initialize(size_t capacity_)
    {
        buffer.resize(capacity_);
        head = 0;
        tail = 0;
        count = 0;
        capacity = capacity_;
    }

    void Enqueue(const std::vector<JointMatrixOperation>& operations)
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

    void ProcessOperations(char* pMappedData, uint32_t discardCount)
    {
        for (size_t i = 0; i < count; ++i) {
            const uint32_t opIndex = (tail + i) % capacity;
            JointMatrixOperation& op = buffer[opIndex];

            if (op.frames == 0) {
                memcpy(pMappedData + op.index * sizeof(Model) + offsetof(Model, modelMatrix), &op.jointMatrix, sizeof(glm::mat4));
            }
            else {
                memcpy(pMappedData + op.index * sizeof(Model) + offsetof(Model, prevModelMatrix), &op.jointMatrix, sizeof(glm::mat4));
                memcpy(pMappedData + op.index * sizeof(Model) + offsetof(Model, modelMatrix), &op.jointMatrix, sizeof(glm::mat4));
            }

            op.frames++;
            if (op.frames == discardCount) {
                tail = (tail + 1) % capacity;
                count--;
            }
        }
    }

private:
    std::vector<JointMatrixOperation> buffer;
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    size_t capacity = 0;
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_OPERATION_RING_BUFFER_H
