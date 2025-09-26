add_executable(crash-handling
        main.cpp
        crash_handler.cpp
        crash_handler.h
        crash_context.cpp
        crash_context.h
        logger.cpp
        logger.h
)

target_include_directories(crash-handling PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/spdlog
)

target_link_libraries(crash-handling PRIVATE
        engine-common
        dbghelp
        psapi
        fmt::fmt
        spdlog::spdlog
)