add_executable(renderer
        main.cpp
        renderer.cpp
        renderer.h
        vulkan_context.cpp
        vulkan_context.h
        imgui_wrapper.cpp
        imgui_wrapper.h
        utils.cpp
        utils.h
        swapchain.cpp
        swapchain.h
        synchronization.cpp
        synchronization.h
)

add_custom_command(TARGET renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:renderer>
)

target_link_libraries(renderer PRIVATE
        engine-common
        crash-handling
        volk
        imgui
        VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        SDL3::SDL3
)