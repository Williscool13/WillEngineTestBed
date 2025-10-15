//
// Created by William on 2025-10-14.
//

#include "body_activation_listener.h"

namespace Physics
{
BodyActivationListener::BodyActivationListener()
{
    activatedEvents.reserve(100);
    deactivatedEvents.reserve(100);
}

void BodyActivationListener::OnBodyActivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData)
{
    std::lock_guard lock(mutex);
    activatedEvents.push_back({inBodyID, inBodyUserData});
}

void BodyActivationListener::OnBodyDeactivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData)
{
    std::lock_guard lock(mutex);
    deactivatedEvents.push_back({inBodyID, inBodyUserData});
}
}
