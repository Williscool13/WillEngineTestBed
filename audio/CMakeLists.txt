add_executable(audio
        main.cpp
        audio.cpp
        audio.h
)


add_custom_command(TARGET audio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:audio>
)


target_link_libraries(audio PRIVATE
        engine-common
        crash-handling
        SDL3::SDL3
)