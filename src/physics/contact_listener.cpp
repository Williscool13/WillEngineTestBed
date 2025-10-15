//
// Created by William on 2025-10-14.
//

#include "contact_listener.h"

#include "JoltPhysics/Jolt/Physics/Body/Body.h"

namespace Physics
{
ContactListener::ContactListener()
{
    deferredEvents.reserve(1000);
}

void ContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
    std::lock_guard lock(mutex);
    deferredEvents.push_back({
        inBody1.GetID(),
        inBody2.GetID(),
        inManifold.mWorldSpaceNormal,
        inManifold.GetWorldSpaceContactPointOn1(0),
        inManifold.mPenetrationDepth
    });
    JPH::ContactListener::OnContactAdded(inBody1, inBody2, inManifold, ioSettings);
}
}
