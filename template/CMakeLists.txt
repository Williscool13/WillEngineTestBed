add_executable(template
        main.cpp
        class-name.cpp
        class-name.h
)

add_custom_command(TARGET template POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:template>
)

target_link_libraries(template PRIVATE
        engine-common
        crash-handling
        render-lib
        input-lib
)
