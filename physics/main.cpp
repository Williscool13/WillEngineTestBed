#include <fmt/format.h>

#include "physics.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Physics (Jolt) ===");


    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/jolt-physics.log");

    Physics::Physics p{};
    p.Initialize();
    p.Run();
    p.Cleanup();

    return 0;
}