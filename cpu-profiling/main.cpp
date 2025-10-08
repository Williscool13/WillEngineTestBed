#include <fmt/format.h>

#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== CPU Profiling ===");


    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/cpu_profiling.log");

    return 0;
}


/*
 *Tracy profiling. Attach with external GUI app.
 * ZoneScoped / ZoneScopedN("name") - scope profiling
 * FrameMark - frame boundaries
 * TracyVkContext - GPU context creation
 * TracyVkZone - GPU command buffer zones
 * TracyVkCollect - end of frame GPU data collection
 * tracy::SetThreadName - thread naming (use this instead of yours for Tracy visibility)
 *
 * Useful but not core:
 *
 * ZoneScopedC(color) - colored zones for visual grouping
 * ZoneName(str, len) - dynamic zone renaming
 * TracyPlot("name", value) - custom metrics (FPS, memory, draw calls)
 * TracyMessage("text", len) - log messages in timeline
 * TracyAlloc/TracyFree - memory tracking
 * TracyLockable(std::mutex, "name") - mutex contention tracking
 * ZoneText(str, len) - add data to current zone
 *
 */