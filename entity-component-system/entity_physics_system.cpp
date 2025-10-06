//
// Created by William on 2025-10-06.
//

#include "entity_physics_system.h"

#include "src/crash-handling/logger.h"

EntityPhysicsSystem::EntityPhysicsSystem()
{
    idPool.reserve(100000);
    for (uint64_t i = 10001; i > 0; --i) { idPool.push_back(i); }
}

void EntityPhysicsSystem::Initialize(entt::registry& reg)
{
    reg.on_construct<RigidBodyEntityComponent>().connect<&EntityPhysicsSystem::OnRigidBodyAdded>(this);
    reg.on_destroy<RigidBodyEntityComponent>().connect<&EntityPhysicsSystem::OnRigidBodyRemoved>(this);
}

void EntityPhysicsSystem::Update(entt::registry& reg, float deltaTime)
{
    const auto view = reg.view<Transform, RigidBodyEntityComponent>();
    for (const auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& rb = view.get<RigidBodyEntityComponent>(entity);

        if (rb.velocity.x > 1.0f) {
            rb.velocity.x = -1.0f;
        }
        else if (rb.velocity.x < -1.0f) {
            rb.velocity.x = 1.0f;
        }
        else {
            rb.velocity.x += 0.1f;
        }

        transform.position.x += rb.velocity.x * deltaTime;
    }
}

void EntityPhysicsSystem::OnRigidBodyAdded(entt::registry& reg, entt::entity entity)
{
    auto& rb = reg.get<RigidBodyEntityComponent>(entity);
    auto& transform = reg.get<Transform>(entity);

    rb.bodyId = CreateJoltBody(rb, transform);
    // if (auto* name = reg.try_get<NameEntityComponent>(entity)) {
    //     LOG_INFO("Physics body created for '{}' ({})", name->name, entt::to_integral(entity));
    // }
    // else {
    //     LOG_INFO("Physics body created for {}", entt::to_integral(entity));
    // }
}

void EntityPhysicsSystem::OnRigidBodyRemoved(entt::registry& reg, entt::entity entity)
{
    auto& rb = reg.get<RigidBodyEntityComponent>(entity);
    DestroyJoltBody(rb.bodyId);
}

BodyId EntityPhysicsSystem::CreateJoltBody(RigidBodyEntityComponent& rb, Transform& t)
{
    if (idPool.empty()) {
        //LOG_ERROR("Physics body ID pool exhausted!");
        return INVALID_BODY_ID;
    }

    // Actually create Jolt body here with transform and body properties from rigidbody.
    const BodyId id = idPool.back();
    idPool.pop_back();

    return id;
}

void EntityPhysicsSystem::DestroyJoltBody(uint64_t id)
{
    idPool.push_back(id);
}
