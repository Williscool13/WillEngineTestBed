//
// Created by William on 2025-11-29.
//

#include "model_serialization.h"

#include "crash-handling/logger_helpers.h"
#include "miniz/miniz.h"

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

bool ModelWriter::AddFile(const std::string& filename, const void* data, size_t size, bool compress)
{
    if (finalized) {
        LOG_INFO("Cannot add files after finalization");
        return false;
    }

    if (filename.length() >= MAX_FILENAME_LENGTH) {
        LOG_INFO("Filename too long: " + filename);
        return false;
    }

    FileEntry entry{};
    std::copy_n(filename.begin(), filename.size(), entry.filename);
    entry.filename[filename.size()] = '\0';
    entry.uncompressedSize = size;
    entry.offset = 0;

    std::vector<uint8_t> buffer;

    if (compress) {
        buffer = CompressZlib(data, size);
        entry.compressedSize = buffer.size();
        entry.compressionType = 1; // zlib
    } else {
        buffer.resize(size);
        std::memcpy(buffer.data(), data, size);
        entry.compressedSize = size;
        entry.compressionType = 0; // none
    }

    fileEntries.push_back(entry);
    fileData.push_back(std::move(buffer));
    return true;
}

void ModelWriter::AddFileFromDisk(const std::string& filename, const std::string& sourcePath, bool compress)
{
    std::ifstream file(sourcePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + sourcePath);
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    AddFile(filename, buffer.data(), buffer.size(), compress);
}

void ModelWriter::Finalize()
{
    if (finalized) return;

    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create output file: " + outputPath);
    }

    uint64_t currentOffset = sizeof(Header);

    for (FileEntry& fileEntry : fileEntries) {
        fileEntry.offset = currentOffset;
        currentOffset += fileEntry.compressedSize;
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


std::vector<uint8_t> CompressZlib(const void* data, size_t size)
{
    mz_ulong compressedSize = mz_compressBound(size);
    std::vector<uint8_t> compressed(compressedSize);

    int result = mz_compress(compressed.data(), &compressedSize, static_cast<const unsigned char*>(data), size);

    if (result != MZ_OK) {
        throw std::runtime_error("Compression failed");
    }

    compressed.resize(compressedSize);
    return compressed;
}

std::vector<uint8_t> DecompressZlib(const void* data, size_t compressedSize, size_t uncompressedSize)
{
    std::vector<uint8_t> decompressed(uncompressedSize);
    mz_ulong destLen = uncompressedSize;

    int result = mz_uncompress(decompressed.data(), &destLen, static_cast<const unsigned char*>(data), compressedSize);

    if (result != MZ_OK) {
        throw std::runtime_error("Decompression failed");
    }

    return decompressed;
}
}
