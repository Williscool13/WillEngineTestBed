//
// Created by William on 2025-11-29.
//

#ifndef WILLENGINETESTBED_MODEL_FORMAT_H
#define WILLENGINETESTBED_MODEL_FORMAT_H

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

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


constexpr char MAGIC[8] = "WILLMDL";
constexpr uint32_t VERSION = 1;
constexpr size_t MAX_FILENAME_LENGTH = 128;

struct FileEntry
{
    char filename[MAX_FILENAME_LENGTH];
    uint64_t offset;
    uint64_t size;
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

    bool AddFile(const std::string& filename, const void* data, size_t size);

    void AddFileFromDisk(const std::string& filename, const std::string& sourcePath);

    void Finalize();

private:
    std::string outputPath;
    std::vector<FileEntry> fileEntries;
    std::vector<std::vector<uint8_t> > fileData;
    bool finalized = false;
};
}

#endif //WILLENGINETESTBED_MODEL_FORMAT_H
