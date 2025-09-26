#include <fmt/format.h>

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

int main()
{
    fmt::println("=== Crash Handler Test ===");

    // Default thread pool settings can be modified *before* creating the async logger:
    // spdlog::init_thread_pool(32768, 1); // queue with max 32k items 1 backing thread.
    auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>("async_file_logger", "logs/async_log.txt", true);
    // alternatively:
    // auto async_file =
    // spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("async_file_logger",
    // "logs/async_log.txt");

    for (int i = 1; i < 101; ++i) {
        async_file->info("Async message #{}", i);
    }

    return 0;
}
