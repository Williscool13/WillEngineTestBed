add_executable(multithreading
        main.cpp
        types.cpp
        types.h
        multithreading.cpp
        multithreading.h
        utils.cpp
        utils.h
)

add_custom_command(TARGET multithreading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:multithreading>
)

target_link_libraries(multithreading PRIVATE
        engine-common
        crash-handling
        SDL3::SDL3
)