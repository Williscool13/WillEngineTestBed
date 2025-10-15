//
// Created by William on 2025-10-12.
//

#ifndef WILLENGINETESTBED_PHYSICS_H
#define WILLENGINETESTBED_PHYSICS_H
#include "body_activation_listener.h"
#include "contact_listener.h"
#include "layer_interface.h"
#include "physics_job_system.h"
#include "TaskScheduler.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSystem.h"

namespace Physics
{
class Physics
{
public:
    Physics() = default;
    ~Physics() = default;

    void Initialize();

    void BasicRun();

    void StressTest();

    void StressTestJoltJobSystem();

    void MegaStressTest();

    void Cleanup();

    PhysicsJobSystem* jobSystem{};
    JPH::JobSystemThreadPool* jobSystem_{};
private:
    BPLayerInterfaceImpl broadPhaseLayerInterface{};
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter{};
    ObjectLayerPairFilterImpl objectLayerPairFilter{};

    BodyActivationListener bodyActivationListener{};
    ContactListener contactListener{};

    JPH::PhysicsSystem physicsSystem{};



    enki::TaskScheduler scheduler{};
    JPH::TempAllocatorImpl* tempAllocator{};
};
} // Physics

#endif //WILLENGINETESTBED_PHYSICS_H
