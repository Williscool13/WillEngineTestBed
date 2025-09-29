cmake_minimum_required(VERSION 4.0)
project(WillEngineTestBed)

set(CMAKE_CXX_STANDARD 20)

add_library(engine-common INTERFACE)
target_compile_definitions(engine-common INTERFACE
        BUILD_CONFIG_NAME="$<CONFIG>"
)

target_include_directories(engine-common INTERFACE
        ${CMAKE_SOURCE_DIR}/extern/json
)

# Dependencies
add_subdirectory(extern/fmt)
set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(extern/spdlog)

add_subdirectory(crash-handling)
add_subdirectory(multithreading)
add_subdirectory(representing-and-manipulating-information)
