add_executable(cpu-profiling
        main.cpp
)

target_link_libraries(cpu-profiling PRIVATE
        engine-common
        crash-handling
)