add_executable(parallelization
        main.cpp
)

set(ENKITS_BUILD_EXAMPLES OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/enkiTS ${CMAKE_BINARY_DIR}/enkiTS)


target_link_libraries(parallelization PRIVATE
        engine-common
        crash-handling
        enkiTS
)