#include <fmt/format.h>

#include "physics.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"
#include "utils.h"

int main()
{
    fmt::println("=== Physics (Jolt) ===");


    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/jolt-physics.log");

    Physics::Physics p{};
    p.Initialize(); {
        ScopedTimer a{"enkiTS Job System Time"};
        //p.BasicRun();
    } {
        ScopedTimer a{"enkiTS Job System Time"};
        p.StressTest();
        // [info] enkiTS Job System Time: 222584 us (222.584 ms)
    } {
        ScopedTimer a{"Jolt Job System Time"};
        p.StressTestJoltJobSystem();
        // [info] Jolt Job System Time: 168462 us (168.462 ms)
    } {
        ScopedTimer a{"Mega Stress Test"};
        // p.MegaStressTest();
        // [info] (Jolt)   Mega Stress Test: 7209668 us (7209.668 ms)
        // [info] (enkiTS) Mega Stress Test: 7538440 us (7538.440 ms)
    }

    // enkiTS is slower (4% at high load, up to 30% on small load) than Jolt, but enkiTS is general purpose and will be used again to parallelize game logic (ECS).

    p.Cleanup();

    return 0;
}
