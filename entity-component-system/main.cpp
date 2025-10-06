#include <fmt/format.h>
#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

#include "types.h"
#include "entity_bounce_system.h"
#include "entity_physics_system.h"
#include "entity_render_system.h"
#include "game_object.h"

void BenchmarkEntityCount(entt::registry& registry, int count)
{
    LOG_INFO("=== Benchmarking with {} entities ===", count);

    EntityPhysicsSystem physicsSystem;
    EntityBounceSystem bounceSystem;
    EntityRenderSystem renderSystem;

    {
        ScopedTimer timer(fmt::format("Entity creation ({})", count));
        for (int i = 0; i < count; ++i) {
            auto entity = registry.create();
            registry.emplace<Transform>(entity);
            registry.emplace<RigidBodyEntityComponent>(entity);

            // 50% have bounce
            if (i % 2 == 0) {
                registry.emplace<BounceEntityComponent>(entity, 0.0f, 10.0f);
            }

            // 75% have render
            if (i % 4 != 0) {
                registry.emplace<RenderEntityComponent>(entity);
            }
        }
    }

    physicsSystem.Initialize(registry);
    bounceSystem.Initialize(registry);
    renderSystem.Initialize(registry);


    constexpr int frameCount = 1000;
    constexpr float dt = 0.016f;

    // Warmup
    for (int i = 0; i < 10; ++i) {
        physicsSystem.Update(registry, dt);
        bounceSystem.Update(registry);
        renderSystem.Update(registry, dt);
    }

    {
        ScopedTimer timer(fmt::format("Full frame update ({} frames)", frameCount));

        {
            ScopedTimer _timer(fmt::format("Physics update ({} frames)", frameCount));
            for (int i = 0; i < frameCount; ++i) {
                physicsSystem.Update(registry, dt);
            }
        }

        {
            ScopedTimer _timer(fmt::format("Bounce update ({} frames)", frameCount));
            for (int i = 0; i < frameCount; ++i) {
                bounceSystem.Update(registry);
            }
        }

        {
            ScopedTimer _timer(fmt::format("Render update ({} frames)", frameCount));
            for (int i = 0; i < frameCount; ++i) {
                renderSystem.Update(registry, dt);
            }
        }
    }


    // Cleanup
    registry.clear();
    LOG_INFO("");
}

void BenchmarkGameObjectCount(int count)
{
    LOG_INFO("=== Benchmarking with {} gameobjects ===", count);

    std::vector<GameObject*> gameObjects;

    {
        ScopedTimer timer(fmt::format("GameObject creation ({})", count));
        gameObjects.reserve(count);
        for (int32_t i = 0; i < count; ++i) {
            gameObjects.push_back(new GameObject());
            auto phys = new PhysicsComponent();
            gameObjects[i]->AddComponent(phys);
            // missing transform component, but transform is embedded in gameobject

            // 50% have bounce
            if (i % 2 == 0) {
                auto bounce = new BounceComponent();
                gameObjects[i]->AddComponent(bounce);
            }

            // 75% have render
            if (i % 4 != 0) {
                auto render = new RenderComponent();
                gameObjects[i]->AddComponent(render);
            }
        }
    }



    constexpr int frameCount = 1000;
    constexpr float dt = 0.016f;

    // Warmup
    for (int i = 0; i < 10; ++i) {
        for (auto gameObject : gameObjects) {
            gameObject->Update(dt);
        }
    }

    {
        ScopedTimer timer(fmt::format("GameObject update ({} frames)", frameCount));
        for (int i = 0; i < frameCount; ++i) {
            for (auto gameObject : gameObjects) {
                gameObject->Update(dt);
            }
        }
    }

    for (auto gob : gameObjects) {
        gob->RemoveAllComponents();
        delete gob;
    }

    gameObjects.clear();


    LOG_INFO("");
}

int main()
{
    fmt::println("=== Entity Component System Benchmark ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/ecs-benchmark.log");

    LOG_INFO("Starting ECS benchmarks");

    entt::registry registry;

    BenchmarkEntityCount(registry, 100);
    BenchmarkGameObjectCount(100);
    BenchmarkEntityCount(registry, 1000);
    BenchmarkGameObjectCount(1000);
    BenchmarkEntityCount(registry, 10000);
    BenchmarkGameObjectCount(10000);
    BenchmarkEntityCount(registry, 100000);
    BenchmarkGameObjectCount(100000);
#ifdef NDEBUG
    BenchmarkEntityCount(registry, 1000000);
    BenchmarkGameObjectCount(1000000);
    BenchmarkEntityCount(registry, 10000000);
    BenchmarkGameObjectCount(10000000);
#endif
    LOG_INFO("Benchmarks complete");
    Logger::Shutdown();


    // Limitations
    /*
     * See results below.
     * Traditional GameObjects are way better (~3.27x) at lower entity/component count.
     * ECS is way better (~2.37x) at high component count, likely due to cache coherency/prefetch optimizations.
     * If we try to project the information, I think game object performance would be significantly worse in a real situation because:
     *  - More Components (even worse cache coherency)
     *  - Disjointed memory locations (if applicable, I suspect in this trivial test setup Game Objects are nearly contiguous)
     *  - Poor ability to parallelize. Much easier to parallelize with ECS.
     *  - GameObject hierarchy is flat in this example, but typical GameObject implementation has deep nesting/recursion, further impacting cache locality
     */

    return 0;
}


/* Results
 * [info] Starting ECS benchmarks
 * [info] === Benchmarking with 100 entities ===
 * [info] Entity creation (100): 61 us (0.061 ms)
 * [info] Physics update (1000 frames): 383 us (0.383 ms)
 * [info] Bounce update (1000 frames): 244 us (0.244 ms)
 * [info] Render update (1000 frames): 562 us (0.562 ms)
 * [info] Full frame update (1000 frames): 1709 us (1.709 ms)
 * [info]
 * [info] === Benchmarking with 100 gameobjects ===
 * [info] GameObject creation (100): 17 us (0.017 ms)
 * [info] GameObject update (1000 frames): 522 us (0.522 ms)
 * [info]
 * [info] === Benchmarking with 1000 entities ===
 * [info] Entity creation (1000): 75 us (0.075 ms)
 * [info] Physics update (1000 frames): 4171 us (4.171 ms)
 * [info] Bounce update (1000 frames): 2768 us (2.768 ms)
 * [info] Render update (1000 frames): 5517 us (5.517 ms)
 * [info] Full frame update (1000 frames): 13035 us (13.035 ms)
 * [info]
 * [info] === Benchmarking with 1000 gameobjects ===
 * [info] GameObject creation (1000): 155 us (0.155 ms)
 * [info] GameObject update (1000 frames): 5628 us (5.628 ms)
 * [info]
 * [info] === Benchmarking with 10000 entities ===
 * [info] Entity creation (10000): 853 us (0.853 ms)
 * [info] Physics update (1000 frames): 53601 us (53.601 ms)
 * [info] Bounce update (1000 frames): 25497 us (25.497 ms)
 * [info] Render update (1000 frames): 55443 us (55.443 ms)
 * [info] Full frame update (1000 frames): 135371 us (135.371 ms)
 * [info]
 * [info] === Benchmarking with 10000 gameobjects ===
 * [info] GameObject creation (10000): 1499 us (1.499 ms)
 * [info] GameObject update (1000 frames): 69213 us (69.213 ms)
 * [info]
 * [info] === Benchmarking with 100000 entities ===
 * [info] Entity creation (100000): 8763 us (8.763 ms)
 * [info] Physics update (1000 frames): 500199 us (500.199 ms)
 * [info] Bounce update (1000 frames): 266994 us (266.994 ms)
 * [info] Render update (1000 frames): 564408 us (564.408 ms)
 * [info] Full frame update (1000 frames): 1332446 us (1332.446 ms)
 * [info]
 * [info] === Benchmarking with 100000 gameobjects ===
 * [info] GameObject creation (100000): 15374 us (15.374 ms)
 * [info] GameObject update (1000 frames): 1380045 us (1380.045 ms)
 * [info]
 * [info] === Benchmarking with 1000000 entities ===
 * [info] Entity creation (1000000): 81737 us (81.737 ms)
 * [info] Physics update (1000 frames): 6108237 us (6108.237 ms)
 * [info] Bounce update (1000 frames): 3987506 us (3987.506 ms)
 * [info] Render update (1000 frames): 5856321 us (5856.321 ms)
 * [info] Full frame update (1000 frames): 15953086 us (15953.086 ms)
 * [info]
 * [info] === Benchmarking with 1000000 gameobjects ===
 * [info] GameObject creation (1000000): 151968 us (151.968 ms)
 * [info] GameObject update (1000 frames): 37918148 us (37918.148 ms)
 * [info]
 * [info] Benchmarks complete
 */


// Rather than manual init/calling systems, use
/*
 * class SystemManager {
 *     std::vector<std::unique_ptr<ISystem>> systems;
 *
 *     void AddSystem<T>() {
 *         systems.push_back(std::make_unique<T>());
 *     }
 *
 *     void Initialize(entt::registry& reg) {
 *         for (auto& sys : systems) sys->Initialize(reg);
 *     }
 *
 *     // Update should separate based on priority/broad phase filtering, ensuring things like Physics runs first, game runs second, etc.
 *     // Updates within a broad phase should not assume any order. Treat the order of updates as completely random
 *     void Update(entt::registry& reg, float dt) {
 *         for (auto& sys : systems) sys->Update(reg, dt);
 *     }
 * };
 */

// For static collection, use macro to avoid having to add system manually
/*
 * class SystemRegistry {
 * public:
 *     static SystemRegistry& Instance() {
 *         static SystemRegistry instance;  // Initialized on first call
 *         return instance;
 *     }
 *
 *     template<typename T>
 *     static bool Register(int priority) {
 *         Instance().RegisterImpl<T>(priority);
 *         return true;
 *     }
 * };
 *
 * // Macro uses Instance(), guaranteeing initialization
 * #define REGISTER_SYSTEM(SystemClass, Priority) \
 * static inline bool _reg_##SystemClass = \
 * SystemRegistry::Register<SystemClass>(Priority);
 */
