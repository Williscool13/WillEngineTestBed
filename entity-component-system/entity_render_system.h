//
// Created by William on 2025-10-06.
//

#ifndef WILLENGINETESTBED_ENTITY_RENDER_SYSTEM_H
#define WILLENGINETESTBED_ENTITY_RENDER_SYSTEM_H


#include <entt/entt.hpp>
#include "types.h"
#include "src/crash-handling/logger.h"

class EntityRenderSystem
{
public:
    void Initialize(entt::registry& reg)
    {
        reg.on_construct<RenderEntityComponent>().connect<&EntityRenderSystem::OnRenderAdded>(this);
        reg.on_destroy<RenderEntityComponent>().connect<&EntityRenderSystem::OnRenderRemoved>(this);
    }

    void Update(entt::registry& reg, float dt)
    {
        auto view = reg.view<RenderEntityComponent>();
        for (auto entity : view) {
            auto& render = view.get<RenderEntityComponent>(entity);

            render.pulseTime += dt;
            float alpha = (sin(render.pulseTime * render.pulseFrequency) + 1.0f) * 0.5f;
            render.color.a = alpha;
        }
    }

private:
    void OnRenderAdded(entt::registry& reg, entt::entity entity)
    {
        // if (auto* name = reg.try_get<NameEntityComponent>(entity)) {
        //     LOG_INFO("Render component added to '{}' ({})", name->name, entt::to_integral(entity));
        // }
        // else {
        //     LOG_INFO("Render component added to {}", entt::to_integral(entity));
        // }
    }

    void OnRenderRemoved(entt::registry& reg, entt::entity entity)
    {
        auto& render = reg.get<RenderEntityComponent>(entity);
    }
};


#endif //WILLENGINETESTBED_ENTITY_RENDER_SYSTEM_H
