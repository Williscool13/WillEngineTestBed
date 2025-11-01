add_library(core-lib STATIC
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