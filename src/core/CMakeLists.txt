add_library(core-lib STATIC
        data-structures/free_list.cpp
        data-structures/free_list.h
        data-structures/handle_allocator.cpp
        data-structures/handle_allocator.h
        data-structures/ring_buffer.cpp
        data-structures/ring_buffer.h
        types/transform.cpp
        types/transform.h
        time.cpp
        time.h
        constants.cpp
        constants.h
)

target_include_directories(core-lib INTERFACE
        ${CMAKE_SOURCE_DIR}/extern/SDL/include
)

add_custom_command(TARGET core-lib POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:renderer>
)

target_link_libraries(core-lib PUBLIC
        engine-common
        SDL3::SDL3
)