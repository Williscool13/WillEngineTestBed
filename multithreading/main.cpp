#include <fmt/format.h>

#include "multithreading.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Multithreading ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/engine.log");

    Multithreading mt{};
    mt.Initialize();
    mt.Run();
    mt.Cleanup();

    return 0;
}
