//
// Created by William on 2025-10-06.
//

#ifndef WILLENGINETESTBED_ENTITY_PHYSICS_SYSTEM_H
#define WILLENGINETESTBED_ENTITY_PHYSICS_SYSTEM_H
#include <cstdint>

#include <entt/entt.hpp>

#include "types.h"

class EntityPhysicsSystem
{
public:
    EntityPhysicsSystem();

    void Initialize(entt::registry& reg);

    void Update(entt::registry& reg, float deltaTime);

private:
    void OnRigidBodyAdded(entt::registry& reg, entt::entity entity);

    void OnRigidBodyRemoved(entt::registry& reg, entt::entity entity);

private:
    BodyId CreateJoltBody(RigidBodyEntityComponent& rb, Transform& t);

    void DestroyJoltBody(uint64_t id);

private:
    std::vector<BodyId> idPool{};
};


#endif //WILLENGINETESTBED_ENTITY_PHYSICS_SYSTEM_H
