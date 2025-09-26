add_executable(logging
        main.cpp
)

target_include_directories(logging PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/spdlog
)

target_link_libraries(logging PRIVATE
        engine-common
        fmt::fmt
        spdlog::spdlog
)
