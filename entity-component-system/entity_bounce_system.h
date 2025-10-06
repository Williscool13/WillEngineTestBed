//
// Created by William on 2025-10-06.
//

#ifndef WILLENGINETESTBED_ENTITY_BOUNCE_SYSTEM_H
#define WILLENGINETESTBED_ENTITY_BOUNCE_SYSTEM_H

#include <entt/entt.hpp>
#include "types.h"
#include "src/crash-handling/logger.h"

class EntityBounceSystem
{
public:
    void Initialize(entt::registry& reg)
    {
        reg.on_construct<BounceEntityComponent>().connect<&EntityBounceSystem::OnBounceAdded>(this);
    }

    void Update(entt::registry& reg)
    {
        auto view = reg.view<Transform, RigidBodyEntityComponent, BounceEntityComponent>();
        for (auto entity : view) {
            auto& transform = view.get<Transform>(entity);
            auto& rb = view.get<RigidBodyEntityComponent>(entity);
            auto& bounce = view.get<BounceEntityComponent>(entity);

            // Oscillate dy between -1 and 1
            if (transform.position.y < bounce.minY && rb.velocity.y < 0) {
                rb.velocity.y = 1.0f;
            } else if (transform.position.y > bounce.maxY && rb.velocity.y > 0) {
                rb.velocity.y = -1.0f;
            }
        }
    }

private:
    void OnBounceAdded(entt::registry& reg, entt::entity entity)
    {
        // if (auto* name = reg.try_get<NameEntityComponent>(entity)) {
        //     LOG_INFO("Bounce behavior added to '{}' ({})", name->name, entt::to_integral(entity));
        // } else {
        //     LOG_INFO("Bounce behavior added to {}", entt::to_integral(entity));
        // }
    }
};


#endif //WILLENGINETESTBED_ENTITY_BOUNCE_SYSTEM_H