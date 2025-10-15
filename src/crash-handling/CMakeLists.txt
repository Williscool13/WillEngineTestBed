add_library(crash-handling STATIC
        crash_handler.cpp
        crash_handler.h
        crash_context.cpp
        crash_context.h
        # todo: logger should be its own lib
        logger.cpp
        logger.h
)

target_link_libraries(crash-handling PUBLIC
        engine-common
        spdlog::spdlog
        dbghelp
        psapi
)