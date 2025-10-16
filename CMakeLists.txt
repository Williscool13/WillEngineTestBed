cmake_minimum_required(VERSION 4.0)
project(WillEngineTestBed)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(NOMINMAX)

# Dependencies
add_subdirectory(extern/fmt)
set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(extern/spdlog)
add_subdirectory(extern/SDL EXCLUDE_FROM_ALL)
set(ENKITS_BUILD_EXAMPLES OFF)
add_subdirectory(extern/enkiTS)
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(extern/volk)
add_subdirectory(extern/vma)
add_subdirectory(extern/vk-bootstrap)

# My Libs
add_subdirectory(src/crash-handling)
add_subdirectory(src/physics)
add_subdirectory(src/render)
add_subdirectory(src/input)

# Imgui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui)
add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_compile_definitions(imgui PUBLIC
        IMGUI_IMPL_VULKAN_USE_VOLK
        VK_NO_PROTOTYPES
)
target_include_directories(imgui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/volk
        ${CMAKE_SOURCE_DIR}/extern/SDL/include
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(imgui PUBLIC
        volk
)

# Jolt
set(PHYSICS_REPO_ROOT "${CMAKE_SOURCE_DIR}/extern/JoltPhysics")
include("${PHYSICS_REPO_ROOT}/Jolt/Jolt.cmake")
target_include_directories(Jolt PUBLIC
        ${PHYSICS_REPO_ROOT}
)
set(CMAKE_CXX_STANDARD 20) # Jolt sets cxx standard to 17
# Common
add_library(engine-common INTERFACE
        src/utils/utils.cpp
        src/utils/utils.h
        )

target_compile_definitions(engine-common INTERFACE
        BUILD_CONFIG_NAME="$<CONFIG>"
)
target_include_directories(engine-common INTERFACE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/extern
)
target_link_libraries(engine-common INTERFACE
        fmt::fmt
)


add_subdirectory(crash-handling-test)
add_subdirectory(multithreading)
add_subdirectory(representing-and-manipulating-information)
add_subdirectory(parallelization)
add_subdirectory(entity-component-system)
add_subdirectory(vulkan)
add_subdirectory(physics)
add_subdirectory(audio)
