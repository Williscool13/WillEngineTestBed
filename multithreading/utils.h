//
// Created by William on 2025-10-07.
//

#ifndef WILLENGINETESTBED_UTILS_H
#define WILLENGINETESTBED_UTILS_H

#include <windows.h>

inline void SetThreadName(const char* name) {
    // Wide string conversion
    wchar_t wideName[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wideName, 256);
    SetThreadDescription(GetCurrentThread(), wideName);
}


#endif //WILLENGINETESTBED_UTILS_H