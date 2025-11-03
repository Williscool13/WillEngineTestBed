//
// Created by William on 2025-11-01.
//

#include "resource_manager.h"

#include "render/model/model_loader.h"

namespace Renderer
{
// ResourceManager::ResourceManager() = default;
//
// ResourceManager::~ResourceManager() = default;
//
// ResourceManager::ResourceManager(VulkanContext* context)
//     : context(context)
// {
//     modelLoader = std::make_unique<ModelLoader>(context);
// }

// ModelEntryHandle ResourceManager::LoadModel(const std::filesystem::path& path)
// {
//     if (auto it = pathToModel.find(path); it != pathToModel.end()) {
//         modelEntries.Get(it->second)->refCount++;
//         return it->second;
//     }
//
//     ModelEntryHandle newModelHandle = modelEntries.Add();
//     ModelEntry* newModel = modelEntries.Get(newModelHandle);
//     newModel->refCount = 1;
//     //newModel->data =;
// }
//
// void ResourceManager::UnloadModel(ModelEntryHandle modelEntryHandle)
// {
//     if (auto* entry = modelEntries.Get(modelEntryHandle)) {
//         if (--entry->refCount == 0) {
//             pathToModel.erase(entry->data.path);
//             modelEntries.Remove(modelEntryHandle);
//         }
//     }
// }
} // Renderer