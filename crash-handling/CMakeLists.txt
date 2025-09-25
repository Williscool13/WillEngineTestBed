if (MSVC)
    add_compile_options(/Zi)
    add_link_options(/DEBUG)
endif ()

add_executable(CrashHandling
        main.cpp
)

# Link Windows crash handling libraries
if (WIN32)
    target_link_libraries(CrashHandling PRIVATE dbghelp psapi fmt::fmt)
endif ()