//
// Created by William on 2025-10-12.
//

#include "physics.h"

#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include "contact_listener.h"
#include "crash_handler.h"
#include "layer_interface.h"
#include "logger.h"

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine)
{
    std::string msg = fmt::format("JPH Assert Failed: {} | {} ({}:{})", inExpression, inMessage, inFile, inLine);
    LOG_ERROR("{}", msg);
    CrashHandler::TriggerManualDump(msg);
    return true;
};
#endif

static void TraceImpl(const char* inFMT, ...)
{
    va_list args;
    va_start(args, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, args);
    va_end(args);

    LOG_DEBUG("[Jolt] {}", buffer);
}

namespace Physics
{
void Physics::Initialize()
{
    // enkiTS
    {
        enki::TaskSchedulerConfig config;
        config.numTaskThreadsToCreate = enki::GetNumHardwareThreads() - 1;
        LOG_INFO("Scheduler operating with {} threads.", config.numTaskThreadsToCreate + 1);
        scheduler.Initialize(config);
    }

    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

    // Todo: enkiTS wrapper for a Jolt job system
    jobSystem = new PhysicsJobSystem(&scheduler, MAX_PHYSICS_JOBS, 8);

    constexpr uint32_t cMaxBodies = 65536;
    constexpr uint32_t cNumBodyMutexes = 0;
    constexpr uint32_t cMaxBodyPairs = 65536;
    constexpr uint32_t cMaxContactConstraints = 10240;
    physicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broadPhaseLayerInterface, objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
    physicsSystem.SetBodyActivationListener(&bodyActivationListener);
    physicsSystem.SetContactListener(&contactListener);

    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    // Force keep alive if ref count is 0
    floorShapeSettings.SetEmbedded();
    JPH::ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
    JPH::ShapeRefC floorShape = floorShapeResult.Get();

    JPH::BodyCreationSettings floorSettings(floorShape, JPH::RVec3(0, -1, 0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::STATIC);
    JPH::Body* floor = bodyInterface.CreateBody(floorSettings);
    bodyInterface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);
    otherBodies.push_back(floor->GetID());

    JPH::BodyCreationSettings sphereSettings(new JPH::SphereShape(0.5f), JPH::RVec3(0.0, 2.0, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::DYNAMIC);
    sphere = bodyInterface.CreateAndAddBody(sphereSettings, JPH::EActivation::Activate);
    bodyInterface.SetLinearVelocity(sphere, JPH::Vec3(0.0f, -5.0f, 0.0f));

    // Expensive, call when streaming new level
    physicsSystem.OptimizeBroadPhase();
}

void Physics::Run()
{
    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    uint32_t step = 0;
    while (true) {
        ++step;

        JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(sphere);
        JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(sphere);
        LOG_INFO("Step {}: Position = ({}, {}, {}), Velocity = ({}, {}, {})", step, position.GetX(), position.GetY(), position.GetZ(), velocity.GetX(), velocity.GetY(), velocity.GetZ());

        // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
        constexpr int cCollisionSteps = 1;
        constexpr float physicsDeltaTime = 1.0f / 60.0f;
        physicsSystem.Update(physicsDeltaTime, cCollisionSteps, tempAllocator, jobSystem);
        jobSystem->ResetTaskPool();

        if (step >= 100) {
            break;
        }
    }
}

void Physics::Cleanup()
{
    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    for (JPH::BodyID bodyId : otherBodies) {
        bodyInterface.RemoveBody(bodyId);
        bodyInterface.DestroyBody(bodyId);
    }
    bodyInterface.RemoveBody(sphere);
    bodyInterface.DestroyBody(sphere);


    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    scheduler.WaitforAllAndShutdown();
}
} // Physics
