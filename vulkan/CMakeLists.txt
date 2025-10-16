add_executable(renderer
        main.cpp
        renderer.cpp
        renderer.h


)

add_custom_command(TARGET renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:renderer>
)

target_link_libraries(renderer PRIVATE
        engine-common
        crash-handling
        render-lib
        input-lib
)