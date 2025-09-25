//
// Created by William on 2025-09-25.
//

#include "crash_context.h"
#include <filesystem>
#include <fstream>
#include <psapi.h>
#include <fmt/format.h>
#include <fmt/chrono.h>

nlohmann::json CrashContext::context;
bool CrashContext::bInitialized = false;

void CrashContext::Initialize()
{
    if (bInitialized) return;

    std::filesystem::create_directories("crashes");

    context = nlohmann::json::object();
    context["session_start"] = GetTimestamp();

    CollectSystemInfo();
    bInitialized = true;

    fmt::println("Crash context initialized");
}

void CrashContext::WriteCrashContext(const std::string& crashReason)
{
    if (!bInitialized) {
        context = nlohmann::json::object();
    }

    context["crash"]["reason"] = crashReason;
    context["crash"]["timestamp"] = GetTimestamp();

    CollectProcessInfo();

    try {
        std::ofstream file("crashes/context.json");
        file << context.dump(2);
        file.close();

        fmt::println("Crash context written to: crashes/context.json");
    } catch (const std::exception& e) {
        fmt::println("Failed to write crash context: {}", e.what());
    }
}

void CrashContext::CollectSystemInfo()
{
    // Memory info
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);

    context["system"]["total_memory_mb"] = memInfo.ullTotalPhys / (1024 * 1024);
    context["system"]["available_memory_mb"] = memInfo.ullAvailPhys / (1024 * 1024);

    // CPU info
    SYSTEM_INFO sysInfo = {};
    GetSystemInfo(&sysInfo);
    context["system"]["cpu_count"] = sysInfo.dwNumberOfProcessors;
}

void CrashContext::CollectProcessInfo()
{
    HANDLE process = GetCurrentProcess();

    // Process memory usage
    PROCESS_MEMORY_COUNTERS memCounters = {};
    if (GetProcessMemoryInfo(process, &memCounters, sizeof(memCounters))) {
        context["process"]["working_set_mb"] = memCounters.WorkingSetSize / (1024 * 1024);
        context["process"]["peak_working_set_mb"] = memCounters.PeakWorkingSetSize / (1024 * 1024);
    }

    // Handle count
    DWORD handleCount = 0;
    GetProcessHandleCount(process, &handleCount);
    context["process"]["handle_count"] = handleCount;
}

std::string CrashContext::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    return fmt::format("{:%Y-%m-%d %H:%M:%S}", now);
}
