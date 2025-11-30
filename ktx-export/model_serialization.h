//
// Created by William on 2025-11-29.
//

#ifndef WILLENGINETESTBED_MODEL_FORMAT_H
#define WILLENGINETESTBED_MODEL_FORMAT_H

#include <string>
#include <vector>
#include <fstream>

#include "render/animation/animation_types.h"
#include "render/model/model_data.h"

namespace Renderer
{
struct ExtractedMeshletModel;
}

namespace KtxExport
{
// Model Geometry
constexpr char MODEL_MAGIC[8] = "WILLGEO";
constexpr uint32_t MODEL_VERSION = 1;

struct ModelBinaryHeader
{
    char magic[8];
    uint32_t version;

    // Counts for each array
    uint32_t vertexCount;
    uint32_t meshletVertexCount;
    uint32_t meshletTriangleCount;
    uint32_t meshletCount;
    uint32_t primitiveCount;
    uint32_t materialCount;
    uint32_t meshCount;
    uint32_t nodeCount;
    uint32_t nodeRemapCount;
    uint32_t animationCount;
    uint32_t inverseBindMatrixCount;
    uint32_t samplerCount;
};

void WriteModelBinary(std::ofstream& file, const Renderer::ExtractedMeshletModel& model);

template<typename T>
void WriteVector(std::ofstream& file, const std::vector<T>& vec)
{
    if (!vec.empty()) {
        file.write(reinterpret_cast<const char*>(vec.data()), vec.size() * sizeof(T));
    }
}

// String serialization
inline void WriteString(std::ofstream& file, const std::string& str)
{
    uint32_t length = static_cast<uint32_t>(str.size());
    file.write(reinterpret_cast<const char*>(&length), sizeof(length));
    if (length > 0) {
        file.write(str.data(), length);
    }
}

inline void ReadString(const uint8_t*& data, std::string& str)
{
    uint32_t length;
    std::memcpy(&length, data, sizeof(length));
    data += sizeof(length);

    str.resize(length);
    if (length > 0) {
        std::memcpy(str.data(), data, length);
        data += length;
    }
}

// Dynamic vector serialization
template<typename T>
void WriteDynamicVector(std::ofstream& file, const std::vector<T>& vec)
{
    auto count = static_cast<uint32_t>(vec.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    if (count > 0) {
        file.write(reinterpret_cast<const char*>(vec.data()), count * sizeof(T));
    }
}

template<typename T>
void ReadDynamicVector(const uint8_t*& data, std::vector<T>& vec)
{
    uint32_t count;
    std::memcpy(&count, data, sizeof(count));
    data += sizeof(count);

    vec.resize(count);
    if (count > 0) {
        std::memcpy(vec.data(), data, count * sizeof(T));
        data += count * sizeof(T);
    }
}

// MeshInformation serialization
inline void WriteMeshInformation(std::ofstream& file, const Renderer::MeshInformation& mesh)
{
    WriteString(file, mesh.name);
    WriteDynamicVector(file, mesh.primitiveIndices);
}

inline void ReadMeshInformation(const uint8_t*& data, Renderer::MeshInformation& mesh)
{
    ReadString(data, mesh.name);
    ReadDynamicVector(data, mesh.primitiveIndices);
}

// Node serialization
inline void WriteNode(std::ofstream& file, const Renderer::Node& node)
{
    WriteString(file, node.name);
    file.write(reinterpret_cast<const char*>(&node.parent), sizeof(node.parent));
    file.write(reinterpret_cast<const char*>(&node.meshIndex), sizeof(node.meshIndex));
    file.write(reinterpret_cast<const char*>(&node.depth), sizeof(node.depth));
    file.write(reinterpret_cast<const char*>(&node.inverseBindIndex), sizeof(node.inverseBindIndex));
    file.write(reinterpret_cast<const char*>(&node.localTranslation), sizeof(node.localTranslation));
    file.write(reinterpret_cast<const char*>(&node.localRotation), sizeof(node.localRotation));
    file.write(reinterpret_cast<const char*>(&node.localScale), sizeof(node.localScale));
}

inline void ReadNode(const uint8_t*& data, Renderer::Node& node)
{
    ReadString(data, node.name);
    std::memcpy(&node.parent, data, sizeof(node.parent));
    data += sizeof(node.parent);
    std::memcpy(&node.meshIndex, data, sizeof(node.meshIndex));
    data += sizeof(node.meshIndex);
    std::memcpy(&node.depth, data, sizeof(node.depth));
    data += sizeof(node.depth);
    std::memcpy(&node.inverseBindIndex, data, sizeof(node.inverseBindIndex));
    data += sizeof(node.inverseBindIndex);
    std::memcpy(&node.localTranslation, data, sizeof(node.localTranslation));
    data += sizeof(node.localTranslation);
    std::memcpy(&node.localRotation, data, sizeof(node.localRotation));
    data += sizeof(node.localRotation);
    std::memcpy(&node.localScale, data, sizeof(node.localScale));
    data += sizeof(node.localScale);
}

// AnimationSampler serialization
inline void WriteAnimationSampler(std::ofstream& file, const Renderer::AnimationSampler& sampler)
{
    WriteDynamicVector(file, sampler.timestamps);
    WriteDynamicVector(file, sampler.values);
    file.write(reinterpret_cast<const char*>(&sampler.interpolation), sizeof(sampler.interpolation));
}

inline void ReadAnimationSampler(const uint8_t*& data, Renderer::AnimationSampler& sampler)
{
    ReadDynamicVector(data, sampler.timestamps);
    ReadDynamicVector(data, sampler.values);
    std::memcpy(&sampler.interpolation, data, sizeof(sampler.interpolation));
    data += sizeof(sampler.interpolation);
}

// Updated Animation serialization
inline void WriteAnimation(std::ofstream& file, const Renderer::Animation& anim)
{
    WriteString(file, anim.name);

    auto samplerCount = static_cast<uint32_t>(anim.samplers.size());
    file.write(reinterpret_cast<const char*>(&samplerCount), sizeof(samplerCount));
    for (const auto& sampler : anim.samplers) {
        WriteAnimationSampler(file, sampler);
    }

    WriteDynamicVector(file, anim.channels);
    file.write(reinterpret_cast<const char*>(&anim.duration), sizeof(anim.duration));
}

inline void ReadAnimation(const uint8_t*& data, Renderer::Animation& anim)
{
    ReadString(data, anim.name);

    // Read samplers with custom serializer
    uint32_t samplerCount;
    std::memcpy(&samplerCount, data, sizeof(samplerCount));
    data += sizeof(samplerCount);

    anim.samplers.resize(samplerCount);
    for (uint32_t i = 0; i < samplerCount; i++) {
        ReadAnimationSampler(data, anim.samplers[i]);
    }

    ReadDynamicVector(data, anim.channels);
    std::memcpy(&anim.duration, data, sizeof(anim.duration));
    data += sizeof(anim.duration);
}


constexpr char MAGIC[8] = "WILLMDL";
constexpr uint32_t VERSION = 1;
constexpr size_t MAX_FILENAME_LENGTH = 128;

struct FileEntry
{
    char filename[MAX_FILENAME_LENGTH];
    uint64_t offset;
    uint64_t compressedSize;
    uint64_t uncompressedSize;
    uint32_t compressionType; // 0 = none, 1 = zlib
};

struct Header
{
    char magic[8];
    uint32_t version;
    uint32_t numFiles;
    uint64_t fileTableOffset;
};

class ModelWriter
{
public:
    explicit ModelWriter(const std::string& path);

    ~ModelWriter();

    bool AddFile(const std::string& filename, const void* data, size_t size, bool compress);

    void AddFileFromDisk(const std::string& filename, const std::string& sourcePath, bool compress);

    void Finalize();

private:
    std::string outputPath;
    std::vector<FileEntry> fileEntries;
    std::vector<std::vector<uint8_t> > fileData;
    bool finalized = false;
};

class ModelReader
{
public:
    explicit ModelReader(const std::string& path);

    ~ModelReader();

    uint32_t GetFileCount() const { return header.numFiles; }

    std::vector<std::string> ListFiles() const;

    bool HasFile(const std::string& filename) const;

    std::vector<uint8_t> ReadFile(const std::string& filename) const;

    bool ReadFile(const std::string& filename, void* buffer, size_t bufferSize) const;

    const FileEntry* GetFileEntry(const std::string& filename) const;

private:
    void ReadHeader();

    void ReadFileTable();

    std::string archivePath;
    mutable std::ifstream file;
    Header header{};
    std::vector<FileEntry> fileEntries;
};

std::vector<uint8_t> CompressZlib(const void* data, size_t size);

std::vector<uint8_t> DecompressZlib(const void* data, size_t compressedSize, size_t uncompressedSize);
}

#endif //WILLENGINETESTBED_MODEL_FORMAT_H
