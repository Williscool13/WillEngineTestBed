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
        Job* job{nullptr};

        PhysicsJobTask()
        {
            m_SetSize = 1;
        }

        explicit PhysicsJobTask(Job* job_) : job(job_)
        {
            m_SetSize = 1;
        }

        void ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_) override;
    };

public:
    PhysicsJobSystem() = default;

    PhysicsJobSystem(enki::TaskScheduler* scheduler, uint32_t maxJobs, uint32_t inMaxBarriers);

    ~PhysicsJobSystem() override = default;

    int GetMaxConcurrency() const override;

    JobHandle CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies) override;

    void ResetTaskPool();

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
    std::atomic<int32_t> mTaskIndex{0};
};


#endif //WILLENGINETESTBED_PHYSICS_JOB_SYSTEM_H
