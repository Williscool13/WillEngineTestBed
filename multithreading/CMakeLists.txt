add_executable(multithreading
        main.cpp
        ${CMAKE_SOURCE_DIR}/crash-handling/crash_handler.cpp
        ${CMAKE_SOURCE_DIR}/crash-handling/crash_handler.h
        ${CMAKE_SOURCE_DIR}/crash-handling/crash_context.cpp
        ${CMAKE_SOURCE_DIR}/crash-handling/crash_context.h
        ${CMAKE_SOURCE_DIR}/crash-handling/logger.cpp
        ${CMAKE_SOURCE_DIR}/crash-handling/logger.h
)

target_include_directories(multithreading PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/spdlog
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(multithreading PRIVATE
        engine-common
        dbghelp
        psapi
        fmt::fmt
        spdlog::spdlog
)