//
// Created by William on 2025-10-14.

#ifndef WILLENGINETESTBED_CONTACT_LISTENER_H
#define WILLENGINETESTBED_CONTACT_LISTENER_H

#include <mutex>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>

struct DeferredCollisionEvent
{
    JPH::BodyID body1;
    JPH::BodyID body2;
    JPH::Vec3 worldNormal;
    JPH::Vec3 contactPoint;
    float penetrationDepth;
};

/**
 * Needs to be thread safe
 */
class ContactListener : public JPH::ContactListener
{
public:
    ContactListener();

    ~ContactListener() override = default;

    void Clear() { deferredEvents.clear(); }

private:
    void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

    std::vector<DeferredCollisionEvent> deferredEvents;
    std::mutex mutex;
};

#endif //WILLENGINETESTBED_CONTACT_LISTENER_H
