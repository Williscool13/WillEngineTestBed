//
// Created by William on 2025-10-12.
//

#ifndef WILLENGINETESTBED_PHYSICS_H
#define WILLENGINETESTBED_PHYSICS_H
#include "body_activation_listener.h"
#include "contact_listener.h"
#include "layer_interface.h"
#include "Jolt/Physics/PhysicsSystem.h"

namespace Physics
{
class Physics
{
public:
    Physics() = default;
    ~Physics() = default;

    void Initialize();

    void Run();

    void Cleanup();

private:
    BPLayerInterfaceImpl broadPhaseLayerInterface{};
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter{};
    ObjectLayerPairFilterImpl objectLayerPairFilter{};

    BodyActivationListener bodyActivationListener{};
    ContactListener contactListener{};

    JPH::PhysicsSystem physicsSystem{};

    JPH::BodyID sphere{};
    std::vector<JPH::BodyID> otherBodies{};
};
} // Physics

#endif //WILLENGINETESTBED_PHYSICS_H
