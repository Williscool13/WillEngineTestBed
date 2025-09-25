//
// Created by William on 2025-09-25.
//

#ifdef WIN32

#include "crash_handler.h"

#include <filesystem>

#include <fmt/format.h>

#include "crash_context.h"
#include "dbghelp.h"


bool CrashHandler::bInitialized = false;
std::string CrashHandler::dumpDir = "crashes/";

void CrashHandler::Initialize(const std::string& dumpDirectory)
{
    if (bInitialized) { return; }
    dumpDir = dumpDirectory;
    std::filesystem::create_directories(dumpDir);

    // #1 Setup exception filter
    SetUnhandledExceptionFilter(ExceptionFilter);
    bInitialized = true;
    fmt::println("Crash handler initialized - dumps go to: {}", dumpDir);
}

LONG CrashHandler::ExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
    std::string filename = dumpDir + "crash_" + GetTimestamp() + ".dmp";

    // #2 Write crash context
    const std::string crashReason = GetExceptionDescription(pExceptionInfo);
    CrashContext::WriteCrashContext(crashReason);

    // #3 Write crash dump
    if (WriteDump(pExceptionInfo, filename)) {
        fmt::println("Crash dump written to {}", filename);
    }
    else {
        fmt::println("Failed to create dump");
    }

    return EXCEPTION_EXECUTE_HANDLER;
}


bool CrashHandler::WriteDump(const PEXCEPTION_POINTERS pExceptionInfo, const std::string& filename)
{
    const HANDLE hFile = CreateFileA(
        filename.c_str(),
        GENERIC_WRITE,
        0, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) return false;

    MINIDUMP_EXCEPTION_INFORMATION mdei = {};
    mdei.ThreadId = GetCurrentThreadId();
    mdei.ExceptionPointers = pExceptionInfo;
    mdei.ClientPointers = FALSE;

    // #3 Meaty function that actually writes to the dmp
    const BOOL success = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        MiniDumpWithDataSegs,
        &mdei,
        nullptr,
        nullptr
    );

    CloseHandle(hFile);
    return success != FALSE;
}

bool CrashHandler::TriggerManualDump(const std::string& reason)
{
    CONTEXT context = {};
    RtlCaptureContext(&context);

    EXCEPTION_RECORD record = {};
    record.ExceptionCode = 0xC0000001;
    record.ExceptionAddress = _ReturnAddress();

    EXCEPTION_POINTERS pointers = {};
    pointers.ExceptionRecord = &record;
    pointers.ContextRecord = &context;

    std::string filename = dumpDir + "manual_" + GetTimestamp() + ".dmp";
    fmt::println("Creating manual dump: {}\nReason: ", filename, reason);

    return WriteDump(&pointers, filename);
}

std::string CrashHandler::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string CrashHandler::GetExceptionDescription(PEXCEPTION_POINTERS pExceptionInfo)
{
    DWORD exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
    void* exceptionAddress = pExceptionInfo->ExceptionRecord->ExceptionAddress;

    std::string description;

    switch (exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
        {
            ULONG_PTR info0 = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR info1 = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];

            if (info0 == 0) {
                description = fmt::format("Access Violation: Read from invalid address 0x{:X}", info1);
            }
            else if (info0 == 1) {
                description = fmt::format("Access Violation: Write to invalid address 0x{:X}", info1);
            }
            else {
                description = "Access Violation: Execute at invalid address";
            }
            break;
        }
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            description = "Array bounds exceeded";
            break;
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            description = "Data type misalignment";
            break;
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            description = "Floating-point denormal operand";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            description = "Floating-point division by zero";
            break;
        case EXCEPTION_FLT_INEXACT_RESULT:
            description = "Floating-point inexact result";
            break;
        case EXCEPTION_FLT_INVALID_OPERATION:
            description = "Floating-point invalid operation";
            break;
        case EXCEPTION_FLT_OVERFLOW:
            description = "Floating-point overflow";
            break;
        case EXCEPTION_FLT_STACK_CHECK:
            description = "Floating-point stack check";
            break;
        case EXCEPTION_FLT_UNDERFLOW:
            description = "Floating-point underflow";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            description = "Illegal instruction";
            break;
        case EXCEPTION_IN_PAGE_ERROR:
            description = "Page-in error";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            description = "Integer division by zero";
            break;
        case EXCEPTION_INT_OVERFLOW:
            description = "Integer overflow";
            break;
        case EXCEPTION_INVALID_DISPOSITION:
            description = "Invalid exception disposition";
            break;
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            description = "Noncontinuable exception";
            break;
        case EXCEPTION_PRIV_INSTRUCTION:
            description = "Privileged instruction";
            break;
        case EXCEPTION_SINGLE_STEP:
            description = "Single step (debugger)";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            description = "Stack overflow";
            break;
        case EXCEPTION_BREAKPOINT:
            description = "Breakpoint hit";
            break;
        default:
            description = fmt::format("Unknown exception (code: 0x{:X})", exceptionCode);
            break;
    }

    description += fmt::format(" at address 0x{:X}", reinterpret_cast<uintptr_t>(exceptionAddress));

    return description;
}

#endif
