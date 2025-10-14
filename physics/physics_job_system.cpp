//
// Created by William on 2025-10-14.
//

#include "physics_job_system.h"

#include <enkiTS/src/TaskScheduler.h>

#include "logger.h"

void PhysicsJobSystem::PhysicsJobTask::ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_)
{
    job->Execute();
    job->Release();
}

PhysicsJobSystem::PhysicsJobSystem(enki::TaskScheduler* scheduler, uint32_t maxJobs, uint32_t inMaxBarriers)
    : scheduler(scheduler)
{
    Init(inMaxBarriers);

    mJobs.Init(maxJobs, maxJobs);
}

int PhysicsJobSystem::GetMaxConcurrency() const
{
    return scheduler->GetNumTaskThreads();
}

JPH::JobHandle PhysicsJobSystem::CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies)
{
    // Copied from JobSystemThreadPool
    JPH_PROFILE_FUNCTION();

    // Loop until we can get a job from the free list
    uint32_t index;
    for (;;) {
        index = mJobs.ConstructObject(inName, inColor, this, inJobFunction, inNumDependencies);
        if (index != AvailableJobs::cInvalidObjectIndex)
            break;
        JPH_ASSERT(false, "No jobs available!");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    Job* job = &mJobs.Get(index);

    // Construct handle to keep a reference, the job is queued below and may immediately complete
    JobHandle handle(job);

    // If there are no dependencies, queue the job now
    if (inNumDependencies == 0)
        QueueJob(job);

    // Return the handle
    return handle;
}

void PhysicsJobSystem::ResetTaskPool()
{
    mTaskIndex = 0;
}

void PhysicsJobSystem::QueueJob(Job* inJob)
{
    inJob->AddRef();

    int idx = mTaskIndex.fetch_add(1);
    JPH_ASSERT(idx < Physics::MAX_PHYSICS_TASKS, "Task pool exhausted");

    // Need to figure out some kind of lock-free container that can be cleared after every physics update.
    mTasks[idx].job = inJob;
    scheduler->AddTaskSetToPipe(&mTasks[idx]);
}

void PhysicsJobSystem::QueueJobs(Job** inJobs, JPH::uint inNumJobs)
{
    JPH_ASSERT(inNumJobs > 0);
    for (Job** job = inJobs,** job_end = inJobs + inNumJobs; job < job_end; ++job)
        QueueJob(*job);
}

void PhysicsJobSystem::FreeJob(Job* inJob)
{
    mJobs.DestructObject(inJob);
}
