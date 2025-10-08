#include <fmt/format.h>
#include <enkiTS/src/TaskScheduler.h>

#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Parallelization ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/enkiTS.log");

    enki::TaskScheduler scheduler;
    scheduler.Initialize();

    LOG_INFO("enkiTS initialized with {} threads", scheduler.GetNumTaskThreads());

    scheduler.WaitforAllAndShutdown();

    return 0;
}
