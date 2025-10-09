add_executable(vulkan
        main.cpp
        renderer.cpp
        renderer.h
)

add_custom_command(TARGET vulkan POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:vulkan>
)

target_link_libraries(vulkan PRIVATE
        engine-common
        crash-handling
        volk
        VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        SDL3::SDL3
)