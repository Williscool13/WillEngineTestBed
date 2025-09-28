#include <fmt/format.h>

#include "crash-handling/crash_context.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

int main()
{
    fmt::println("=== Multithreading ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/engine.log");

    return 0;
}
