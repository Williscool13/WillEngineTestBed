//
// Created by William on 2025-11-29.
//

#include "model_serialization.h"

#include "render/model/model_data.h"

namespace KtxExport
{
void WriteModelBinary(std::ofstream& file, const Renderer::ExtractedMeshletModel& model)
{
    ModelBinaryHeader header{};
    std::memcpy(header.magic, MODEL_MAGIC, 8);
    header.version = MODEL_VERSION;
    header.vertexCount = static_cast<uint32_t>(model.vertices.size());
    header.meshletVertexCount = static_cast<uint32_t>(model.meshletVertices.size());
    header.meshletTriangleCount = static_cast<uint32_t>(model.meshletTriangles.size());
    header.meshletCount = static_cast<uint32_t>(model.meshlets.size());
    header.primitiveCount = static_cast<uint32_t>(model.primitives.size());
    header.materialCount = static_cast<uint32_t>(model.materials.size());
    header.meshCount = static_cast<uint32_t>(model.allMeshes.size());
    header.nodeCount = static_cast<uint32_t>(model.nodes.size());
    header.nodeRemapCount = static_cast<uint32_t>(model.nodeRemap.size());
    header.animationCount = static_cast<uint32_t>(model.animations.size());
    header.inverseBindMatrixCount = static_cast<uint32_t>(model.inverseBindMatrices.size());
    header.samplerCount = static_cast<uint32_t>(model.samplerInfos.size());

    file.write(reinterpret_cast<const char*>(&header), sizeof(ModelBinaryHeader));
    WriteVector(file, model.vertices);
    WriteVector(file, model.meshletVertices);
    WriteVector(file, model.meshletTriangles);
    WriteVector(file, model.meshlets);
    WriteVector(file, model.primitives);
    WriteVector(file, model.materials);
    WriteVector(file, model.allMeshes);
    WriteVector(file, model.nodes);
    WriteVector(file, model.nodeRemap);
    WriteVector(file, model.animations);
    WriteVector(file, model.inverseBindMatrices);
    WriteVector(file, model.samplerInfos);
}

ModelWriter::ModelWriter(const std::string& path)
    : outputPath(path)
{}

ModelWriter::~ModelWriter()
{
    if (!finalized) {
        Finalize();
    }
}

bool ModelWriter::AddFile(const std::string& filename, const void* data, size_t size)
{
    if (finalized) {
        return false;
    }

    if (filename.length() >= MAX_FILENAME_LENGTH) {
        return false;
    }

    FileEntry entry{};

    std::copy_n(filename.begin(), filename.size(), entry.filename);
    entry.filename[filename.size()] = '\0';

    entry.size = size;
    entry.offset = 0;
    fileEntries.push_back(entry);

    std::vector<uint8_t> buffer(size);
    std::memcpy(buffer.data(), data, size);
    fileData.push_back(std::move(buffer));
    return true;
}

void ModelWriter::AddFileFromDisk(const std::string& filename, const std::string& sourcePath)
{
    std::ifstream file(sourcePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + sourcePath);
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    AddFile(filename, buffer.data(), buffer.size());
}

void ModelWriter::Finalize()
{
    if (finalized) return;

    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create output file: " + outputPath);
    }

    uint64_t currentOffset = sizeof(Header);

    for (size_t i = 0; i < fileEntries.size(); i++) {
        fileEntries[i].offset = currentOffset;
        currentOffset += fileEntries[i].size;
    }

    uint64_t fileTableOffset = currentOffset;

    Header header{};
    std::memcpy(header.magic, MAGIC, 8);
    header.version = VERSION;
    header.numFiles = static_cast<uint32_t>(fileEntries.size());
    header.fileTableOffset = fileTableOffset;
    file.write(reinterpret_cast<const char*>(&header), sizeof(Header));

    for (const auto& data : fileData) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    for (const auto& entry : fileEntries) {
        file.write(reinterpret_cast<const char*>(&entry), sizeof(FileEntry));
    }

    finalized = true;
}
}
