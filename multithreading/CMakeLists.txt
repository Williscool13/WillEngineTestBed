add_executable(multithreading
        main.cpp
)

target_link_libraries(multithreading PRIVATE
        engine-common
        crash-handling
)