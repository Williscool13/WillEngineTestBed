add_executable(crash-handling-test
        main.cpp
)

target_link_libraries(crash-handling-test PRIVATE
        engine-common
        crash-handling
)