//
// Created by William on 2025-10-14.
//

#ifndef WILLENGINETESTBED_BODY_ACTIVATION_LISTENER_H
#define WILLENGINETESTBED_BODY_ACTIVATION_LISTENER_H

#include <mutex>
#include <Jolt/Jolt.h>

#include "Jolt/Physics/Body/BodyActivationListener.h"
#include "Jolt/Physics/Body/BodyID.h"

struct DeferredBodyActivationEvent
{
    JPH::BodyID bodyId;
    uint64_t bodyUserData;
};

class BodyActivationListener : public JPH::BodyActivationListener
{
public:
    BodyActivationListener();

    ~BodyActivationListener() override = default;

    void Clear() { activatedEvents.clear(); deactivatedEvents.clear(); }

private:
    void OnBodyActivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override;

    void OnBodyDeactivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override;

    std::vector<DeferredBodyActivationEvent> activatedEvents;
    std::vector<DeferredBodyActivationEvent> deactivatedEvents;
    std::mutex mutex;
};


#endif //WILLENGINETESTBED_BODY_ACTIVATION_LISTENER_H
