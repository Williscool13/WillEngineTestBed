//
// Created by William on 2025-09-25.
//

#ifndef WILLENGINETESTBED_CRASH_CONTEXT_H
#define WILLENGINETESTBED_CRASH_CONTEXT_H

#include <json/nlohmann/json.hpp>

#include <Windows.h>

class CrashContext {
public:
    static void Initialize();
    static void WriteCrashContext(const std::string& crashReason, const std::string& folderPath);

private:
    static void CollectSystemInfo();
    static void CollectProcessInfo();
    static nlohmann::json GetBuildConfiguration();
    static std::string GetTimestamp();

    static nlohmann::ordered_json context;
    static bool bInitialized;
};


#endif //WILLENGINETESTBED_CRASH_CONTEXT_H