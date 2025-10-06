cmake_minimum_required(VERSION 4.0)
project(WillEngineTestBed)

set(CMAKE_CXX_STANDARD 20)

# Dependencies
add_subdirectory(extern/fmt)
set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(extern/spdlog)

add_library(engine-common INTERFACE)
target_compile_definitions(engine-common INTERFACE
        BUILD_CONFIG_NAME="$<CONFIG>"
)

target_include_directories(engine-common INTERFACE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/extern
)

target_link_libraries(engine-common INTERFACE
        fmt::fmt
)

add_library(crash-handling STATIC
        ${CMAKE_SOURCE_DIR}/src/crash-handling/crash_handler.cpp
        ${CMAKE_SOURCE_DIR}/src/crash-handling/crash_handler.h
        ${CMAKE_SOURCE_DIR}/src/crash-handling/crash_context.cpp
        ${CMAKE_SOURCE_DIR}/src/crash-handling/crash_context.h
        ${CMAKE_SOURCE_DIR}/src/crash-handling/logger.cpp
        ${CMAKE_SOURCE_DIR}/src/crash-handling/logger.h
)

target_link_libraries(crash-handling PUBLIC
        engine-common
        dbghelp
        psapi
        spdlog::spdlog
)


add_subdirectory(crash-handling-test)
#add_subdirectory(multithreading)
add_subdirectory(representing-and-manipulating-information)
#add_subdirectory(cache-coherency)
add_subdirectory(entity-component-system)
