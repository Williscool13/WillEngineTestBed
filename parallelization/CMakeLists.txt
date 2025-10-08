add_executable(parallelization
        main.cpp
)


target_link_libraries(parallelization PRIVATE
        engine-common
        crash-handling
        enkiTS
)