add_executable(CrashHandling
        main.cpp
        crash_handler.cpp
        crash_handler.h
        crash_context.cpp
        crash_context.h
)

target_compile_definitions(CrashHandling PRIVATE
        BUILD_CONFIG_NAME="$<CONFIG>"
)

target_include_directories(CrashHandling PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/json
)

if (WIN32)
    target_link_libraries(CrashHandling PRIVATE dbghelp psapi fmt::fmt)
endif ()