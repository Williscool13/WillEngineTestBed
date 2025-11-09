add_executable(multi-buffering
        main.cpp
        multi-buffering.cpp
        multi-buffering.h
)

add_custom_command(TARGET multi-buffering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:multi-buffering>
)

target_link_libraries(multi-buffering PRIVATE
        engine-common
        crash-handling
        render-lib
        input-lib
)
