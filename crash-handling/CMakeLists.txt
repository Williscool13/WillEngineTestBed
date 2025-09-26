add_executable(crash-handling
        main.cpp
        crash_handler.cpp
        crash_handler.h
        crash_context.cpp
        crash_context.h
)

target_link_libraries(crash-handling PRIVATE
        engine-common
        dbghelp
        psapi
        fmt::fmt)