//
// Created by William on 2025-11-01.
//

#ifndef WILLENGINETESTBED_RESOURCE_MANAGER_H
#define WILLENGINETESTBED_RESOURCE_MANAGER_H
#include <memory>
#include <unordered_map>

#include "model/model_data.h"


namespace Renderer
{
struct VulkanContext;
class ModelLoader;



// class ResourceManager
// {
//     ResourceManager();
//
//     ~ResourceManager();
//
//     ResourceManager(VulkanContext* context);
//
//     ModelEntryHandle LoadModel(const std::filesystem::path& path);
//
//     void UnloadModel(ModelEntryHandle modelEntryHandle);
//
// private:
//     VulkanContext* context{};
//
//     std::unique_ptr<ModelLoader> modelLoader{};
//
//     FreeList<ModelEntry, 1024> modelEntries;
//     std::unordered_map<std::filesystem::path, ModelEntryHandle> pathToModel;
// };
} // Renderer

#endif //WILLENGINETESTBED_RESOURCE_MANAGER_H
