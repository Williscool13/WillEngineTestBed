//
// Created by William on 2025-10-14.
//

#ifndef WILLENGINETESTBED_PHYSICS_JOB_SYSTEM_H
#define WILLENGINETESTBED_PHYSICS_JOB_SYSTEM_H

#include <Jolt/Jolt.h>

#include "physics_constants.h"
#include "enkiTS/src/TaskScheduler.h"
#include "Jolt/Core/FixedSizeFreeList.h"
#include "Jolt/Core/JobSystemWithBarrier.h"

class PhysicsJobSystem : public JPH::JobSystemWithBarrier
{
    struct PhysicsJobTask final : enki::ITaskSet
    {
        std::vector<Job*> jobs;

        PhysicsJobTask()
        {
            jobs.reserve(16);
            m_SetSize = 0;
        }

        void ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_) override
        {
            for (uint32_t i = range_.start; i < range_.end; ++i) {
                jobs[i]->Execute();
                jobs[i]->Release();
            }
        }

        void Reset()
        {
            jobs.clear();
            m_SetSize = 0;
        }
    };

public:
    PhysicsJobSystem() = default;

    PhysicsJobSystem(enki::TaskScheduler* scheduler, uint32_t maxJobs, uint32_t inMaxBarriers);

    ~PhysicsJobSystem() override = default;

    int32_t GetMaxConcurrency() const override;

    JobHandle CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies) override;

    uint64_t ResetTaskPool();

protected:
    void QueueJob(Job* inJob) override;

    void QueueJobs(Job** inJobs, JPH::uint inNumJobs) override;

    void FreeJob(Job* inJob) override;

private:
    enki::TaskScheduler* scheduler;

    /// Array of jobs (fixed size)
    using AvailableJobs = JPH::FixedSizeFreeList<Job>;
    AvailableJobs mJobs;
    PhysicsJobTask mTasks[Physics::MAX_PHYSICS_TASKS];
    std::atomic<uint64_t> mTaskIndex{0};
};


#endif //WILLENGINETESTBED_PHYSICS_JOB_SYSTEM_H
