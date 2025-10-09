//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_RENDERER_H
#define WILLENGINETESTBED_RENDERER_H

#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL.h>

namespace Renderer
{
struct DeviceInfo
{
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProps{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};
};

class Renderer
{
public:
    static DeviceInfo deviceInfo;

    Renderer() = default;

    void Initialize();

    void Run();

    void Cleanup();

private:
    SDL_Window* window;

    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    uint32_t graphicsQueueFamily{};
    VmaAllocator allocator{};
    VkDebugUtilsMessengerEXT debugMessenger{};

    bool bShouldExit;
};
}


#endif //WILLENGINETESTBED_RENDERER_H
