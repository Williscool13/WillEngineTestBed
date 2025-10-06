#include <fmt/format.h>

#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

long cnt = 0;

void ThreadFunc(long niters) {
    for (long i = 0; i < niters; i++) {
        cnt++;
    }
}

int main()
{
    fmt::println("=== Multithreading ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/engine.log");

    long niters = 10000000;

    std::thread t1(ThreadFunc, niters);
    std::thread t2(ThreadFunc, niters);

    t1.join();
    t2.join();

    fmt::println("final cnt: {}", cnt);

    return 0;
}
