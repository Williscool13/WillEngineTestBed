add_executable(single-buffering
        main.cpp
        single-buffering.cpp
        single-buffering.h
)

add_custom_command(TARGET single-buffering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:single-buffering>
)

target_link_libraries(single-buffering PRIVATE
        engine-common
        crash-handling
        render-lib
        input-lib
)
