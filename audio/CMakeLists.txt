add_executable(audio
        main.cpp
        audio.cpp
        audio.h
        audio_system.cpp
        audio_system.h
        audio_source.cpp
        audio_source.h
        audio_clip.cpp
        audio_clip.h
        audio_constants.cpp
        audio_constants.h
        audio_utils.cpp
        audio_utils.h
        audio_types.cpp
        audio_types.h
        load_audio_clip_task.cpp
        load_audio_clip_task.h
        audio_commands.cpp
        audio_commands.h
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
        enkiTS
)