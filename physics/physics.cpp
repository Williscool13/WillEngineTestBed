//
// Created by William on 2025-10-12.
//

#include "physics.h"

#include <Jolt/Jolt.h>

#include "glm/glm.hpp"

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

    // the additional 4 is for mega stress test only
    tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024 * 1 << 2);
    jobSystem = new PhysicsJobSystem(&scheduler, MAX_PHYSICS_JOBS, 8);
    jobSystem_ = new JPH::JobSystemThreadPool(MAX_PHYSICS_JOBS, 8, std::thread::hardware_concurrency() - 1);

    constexpr uint32_t cMaxBodies = 65536;
    constexpr uint32_t cNumBodyMutexes = 0;
    constexpr uint32_t cMaxBodyPairs = 65536;
    // the additional 32 is for mega stress test only
    constexpr uint32_t cMaxContactConstraints = 1024 * 1 << 5;
    physicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broadPhaseLayerInterface, objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
    physicsSystem.SetBodyActivationListener(&bodyActivationListener);
    physicsSystem.SetContactListener(&contactListener);
}

void Physics::BasicRun()
{
    JPH::BodyID sphere{};
    std::vector<JPH::BodyID> otherBodies{};

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
        bodyActivationListener.Clear();
        contactListener.Clear();

        if (step >= 1000) {
            break;
        }
    }

    for (JPH::BodyID bodyId : otherBodies) {
        bodyInterface.RemoveBody(bodyId);
        bodyInterface.DestroyBody(bodyId);
    }
    bodyInterface.RemoveBody(sphere);
    bodyInterface.DestroyBody(sphere);
}


void Physics::StressTest()
{
    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    JPH::ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
    JPH::ShapeRefC floorShape = floorShapeResult.Get();
    JPH::BodyCreationSettings floorSettings(floorShape, JPH::RVec3(0, -1, 0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::STATIC);
    JPH::BodyID floorId = bodyInterface.CreateAndAddBody(floorSettings, JPH::EActivation::DontActivate);

    // Pyramid of boxes (N layers)
    constexpr int pyramidLayers = 20;
    std::vector<JPH::BodyID> boxes{};

    JPH::BoxShapeSettings boxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    for (int32_t layer = 0; layer < pyramidLayers; ++layer) {
        for (int32_t x = 0; x <= layer; ++x) {
            float xPos = (x - layer / 2.0f) * 1.1f;
            float yPos = 1.0f + layer * 1.1f;

            JPH::BodyCreationSettings settings(
                boxShape.Create().Get(),
                JPH::RVec3(xPos, yPos, 0),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Dynamic,
                Layers::DYNAMIC
            );

            boxes.push_back(bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate));
        }
    }


    // Wrecking ball
    JPH::SphereShapeSettings sphereSettings(2.0f);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();
    JPH::ShapeRefC sphereShape = sphereResult.Get();
    JPH::BodyCreationSettings ballSettings(
        sphereShape,
        JPH::RVec3(-15, 10, 0),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::DYNAMIC
    );
    JPH::BodyID ball = bodyInterface.CreateAndAddBody(ballSettings, JPH::EActivation::Activate);
    bodyInterface.SetLinearVelocity(ball, JPH::Vec3(30.0f, 0.0f, 0.0f));

    physicsSystem.OptimizeBroadPhase();

    uint32_t activeBodies = boxes.size() + 1;
    for (int step = 0; step < 500 && activeBodies > 10; ++step) {
        if (step % 50 == 0) {
            LOG_INFO("Step {}: {} active bodies", step, activeBodies);
        }

        if (step % 200 == 0) {
            JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(ball);
            JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(ball);
            //LOG_INFO("Sphere:  Position = ({}, {}, {}), Velocity = ({}, {}, {})", position.GetX(), position.GetY(), position.GetZ(), velocity.GetX(), velocity.GetY(), velocity.GetZ());
        }
        physicsSystem.Update(1.0f / 60.0f, 1, tempAllocator, jobSystem);
        uint64_t taskMax = jobSystem->ResetTaskPool();
        if (step % 50 == 0) {
            LOG_INFO("Max task index was: {}", taskMax);
        }


        bodyActivationListener.Clear();
        contactListener.Clear();

        activeBodies = 0;
        for (auto id : boxes) {
            if (bodyInterface.IsActive(id)) { ++activeBodies; }
        }
    }

    for (JPH::BodyID bodyId : boxes) {
        bodyInterface.RemoveBody(bodyId);
        bodyInterface.DestroyBody(bodyId);
    }
    bodyInterface.RemoveBody(ball);
    bodyInterface.DestroyBody(ball);
    bodyInterface.RemoveBody(floorId);
    bodyInterface.DestroyBody(floorId);
}

void Physics::StressTestJoltJobSystem()
{

    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    JPH::ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
    JPH::ShapeRefC floorShape = floorShapeResult.Get();
    JPH::BodyCreationSettings floorSettings(floorShape, JPH::RVec3(0, -1, 0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::STATIC);
    JPH::BodyID floorId = bodyInterface.CreateAndAddBody(floorSettings, JPH::EActivation::DontActivate);

    // Pyramid of boxes (N layers)
    constexpr int pyramidLayers = 20;
    std::vector<JPH::BodyID> boxes{};

    JPH::BoxShapeSettings boxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    for (int32_t layer = 0; layer < pyramidLayers; ++layer) {
        for (int32_t x = 0; x <= layer; ++x) {
            float xPos = (x - layer / 2.0f) * 1.1f;
            float yPos = 1.0f + layer * 1.1f;

            JPH::BodyCreationSettings settings(
                boxShape.Create().Get(),
                JPH::RVec3(xPos, yPos, 0),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Dynamic,
                Layers::DYNAMIC
            );

            boxes.push_back(bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate));
        }
    }


    // Wrecking ball
    JPH::SphereShapeSettings sphereSettings(2.0f);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();
    JPH::ShapeRefC sphereShape = sphereResult.Get();
    JPH::BodyCreationSettings ballSettings(
        sphereShape,
        JPH::RVec3(-15, 10, 0),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::DYNAMIC
    );
    JPH::BodyID ball = bodyInterface.CreateAndAddBody(ballSettings, JPH::EActivation::Activate);
    bodyInterface.SetLinearVelocity(ball, JPH::Vec3(30.0f, 0.0f, 0.0f));

    physicsSystem.OptimizeBroadPhase();

    uint32_t activeBodies = boxes.size() + 1;
    for (int step = 0; step < 500 && activeBodies > 10; ++step) {
        if (step % 50 == 0) {
            LOG_INFO("Step {}: {} active bodies", step, activeBodies);
        }

        if (step % 200 == 0) {
            JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(ball);
            JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(ball);
            //LOG_INFO("Sphere:  Position = ({}, {}, {}), Velocity = ({}, {}, {})", position.GetX(), position.GetY(), position.GetZ(), velocity.GetX(), velocity.GetY(), velocity.GetZ());
        }
        physicsSystem.Update(1.0f / 60.0f, 1, tempAllocator, jobSystem_);
        // if (step % 50 == 0) {
        //     LOG_INFO("Max task index was: {}", taskMax);
        // }


        bodyActivationListener.Clear();
        contactListener.Clear();

        activeBodies = 0;
        for (auto id : boxes) {
            if (bodyInterface.IsActive(id)) { ++activeBodies; }
        }
    }

    for (JPH::BodyID bodyId : boxes) {
        bodyInterface.RemoveBody(bodyId);
        bodyInterface.DestroyBody(bodyId);
    }
    bodyInterface.RemoveBody(ball);
    bodyInterface.DestroyBody(ball);
    bodyInterface.RemoveBody(floorId);
    bodyInterface.DestroyBody(floorId);
}

void Physics::MegaStressTest()
{
    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    // Large floor
    JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(200.0f, 1.0f, 200.0f));
    JPH::ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
    JPH::ShapeRefC floorShape = floorShapeResult.Get();
    JPH::BodyCreationSettings floorSettings(floorShape, JPH::RVec3(0, -1, 0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::STATIC);
    JPH::BodyID floorId = bodyInterface.CreateAndAddBody(floorSettings, JPH::EActivation::DontActivate);

    std::vector<JPH::BodyID> allBodies;

    // Multiple pyramids across the floor
    JPH::BoxShapeSettings boxShapeSettings(JPH::Vec3(0.5f, 0.5f, 0.5f));
    JPH::ShapeSettings::ShapeResult boxResult = boxShapeSettings.Create();
    JPH::ShapeRefC boxShape = boxResult.Get();

    constexpr int pyramidLayers = 15;
    constexpr int numPyramids = 5;

    for (int p = 0; p < numPyramids; ++p) {
        float pyramidOffsetX = (p - numPyramids / 2) * 20.0f;

        for (int32_t layer = 0; layer < pyramidLayers; ++layer) {
            for (int32_t x = 0; x <= layer; ++x) {
                for (int32_t z = 0; z <= layer; ++z) {
                    float xPos = pyramidOffsetX + (x - layer / 2.0f) * 1.1f;
                    float yPos = 1.0f + layer * 1.1f;
                    float zPos = (z - layer / 2.0f) * 1.1f;

                    JPH::BodyCreationSettings settings(
                        boxShape,
                        JPH::RVec3(xPos, yPos, zPos),
                        JPH::Quat::sIdentity(),
                        JPH::EMotionType::Dynamic,
                        Layers::DYNAMIC
                    );
                    settings.mAllowSleeping = false; // Force all to stay active

                    allBodies.push_back(bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate));
                }
            }
        }
    }

    // Rain of spheres
    JPH::SphereShapeSettings sphereSettings(0.8f);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();
    JPH::ShapeRefC sphereShape = sphereResult.Get();

    constexpr int numSpheres = 500;
    std::vector<JPH::BodyID> spheres;

    for (int i = 0; i < numSpheres; ++i) {
        float xPos = ((i % 25) - 12) * 3.0f;
        float zPos = ((i / 25) - 10) * 3.0f;
        float yPos = 30.0f + (i % 10) * 2.0f;

        JPH::BodyCreationSettings ballSettings(
            sphereShape,
            JPH::RVec3(xPos, yPos, zPos),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            Layers::DYNAMIC
        );
        ballSettings.mRestitution = 0.6f;
        ballSettings.mAllowSleeping = false;

        spheres.push_back(bodyInterface.CreateAndAddBody(ballSettings, JPH::EActivation::Activate));
        allBodies.push_back(spheres.back());
    }

    // Large wrecking balls
    JPH::SphereShapeSettings bigSphereSettings(3.0f);
    JPH::ShapeSettings::ShapeResult bigSphereResult = bigSphereSettings.Create();
    JPH::ShapeRefC bigSphereShape = bigSphereResult.Get();

    std::vector<JPH::BodyID> wreckingBalls;
    for (int i = 0; i < 3; ++i) {
        JPH::BodyCreationSettings bigBallSettings(
            bigSphereShape,
            JPH::RVec3(-50 + i * 50, 20, -80),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            Layers::DYNAMIC
        );
        bigBallSettings.mAllowSleeping = false;

        JPH::BodyID ball = bodyInterface.CreateAndAddBody(bigBallSettings, JPH::EActivation::Activate);
        bodyInterface.SetLinearVelocity(ball, JPH::Vec3(0.0f, 0.0f, 50.0f));
        wreckingBalls.push_back(ball);
        allBodies.push_back(ball);
    }

    LOG_INFO("Created {} total bodies", allBodies.size());
    physicsSystem.OptimizeBroadPhase();

    uint64_t maxTasksEver = 0;
    for (int step = 0; step < 1000; ++step) {
        physicsSystem.Update(1.0f / 60.0f, 1, tempAllocator, jobSystem);
        uint64_t taskMax = jobSystem->ResetTaskPool();
        maxTasksEver = glm::max(maxTasksEver, taskMax);

        if (step % 100 == 0) {
            uint32_t activeBodies = 0;
            for (auto id : allBodies) {
                if (bodyInterface.IsActive(id)) ++activeBodies;
            }
            LOG_INFO("Step {}: {} active bodies, {} tasks this frame, {} max tasks", step, activeBodies, taskMax, maxTasksEver);
        }

        bodyActivationListener.Clear();
        contactListener.Clear();
    }

    LOG_INFO("Cleanup {} bodies", allBodies.size());
    for (JPH::BodyID bodyId : allBodies) {
        bodyInterface.RemoveBody(bodyId);
        bodyInterface.DestroyBody(bodyId);
    }
    bodyInterface.RemoveBody(floorId);
    bodyInterface.DestroyBody(floorId);
}

void Physics::Cleanup()
{
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    scheduler.WaitforAllAndShutdown();
}
} // Physics
