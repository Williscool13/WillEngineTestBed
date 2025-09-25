//
// Created by William on 2025-09-25.
//

#ifndef WILLENGINETESTBED_CRASH_HANDLER_H
#define WILLENGINETESTBED_CRASH_HANDLER_H

#ifdef WIN32
#include <string>

#include <Windows.h>

class CrashHandler
{
public:
    static void Initialize(const std::string& dumpDirectory = "crashes/");

    static bool TriggerManualDump(const std::string& reason = "Manual");

private:
    static LONG WINAPI ExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo);

    static bool WriteDump(PEXCEPTION_POINTERS pExceptionInfo, const std::string& filename);

    static std::string GetTimestamp();

    static std::string GetExceptionDescription(PEXCEPTION_POINTERS pExceptionInfo);

    static std::string CreateCrashFolder();

private:

    static std::string s_baseDumpDirectory;
    static std::string s_currentCrashFolder;
    static bool bInitialized;
};
#endif

#endif //WILLENGINETESTBED_CRASH_HANDLER_H
