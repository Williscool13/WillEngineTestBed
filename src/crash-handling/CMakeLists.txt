add_library(crash-handling STATIC
        crash_handler.cpp
        crash_handler.h
        crash_context.cpp
        crash_context.h
        logger.cpp
        logger.h
)

if(NOT TARGET fmt::fmt)
    add_subdirectory(extern/fmt)
endif()

if(NOT TARGET spdlog::spdlog)
    set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
    add_subdirectory(extern/spdlog)
endif()

target_link_libraries(crash-handling PUBLIC
        fmt::fmt
        spdlog::spdlog
        dbghelp
        psapi
)

target_include_directories(crash-handling PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/extern
)