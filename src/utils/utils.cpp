//
// Created by William on 2025-10-15.
//

#include "utils.h"

#include <filesystem>

#include "crash-handling/logger_helpers.h"


namespace Utils
{
ScopedTimer::~ScopedTimer()
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    LOG_INFO("{}: {} us ({:.3f} ms)", name, duration.count(), duration.count() / 1000.0);
}

FrameTimeTracker::FrameTimeTracker(size_t historySize, float spikeThreshold)
    : historySize(std::min(historySize, MAX_HISTORY_SIZE))
      , spikeThreshold(spikeThreshold)
{}

void FrameTimeTracker::RecordFrameTime(float frameTimeMs)
{
    history[currentIndex] = frameTimeMs;
    currentIndex = (currentIndex + 1) % historySize;

    if (sampleCount < historySize) {
        sampleCount++;
    }

    UpdateRollingAverage();
}

float FrameTimeTracker::GetLatestFrameTime() const
{
    if (sampleCount == 0) return 0.0f;

    size_t latestIndex = (currentIndex == 0) ? historySize - 1 : currentIndex - 1;
    return history[latestIndex];
}

void FrameTimeTracker::UpdateRollingAverage()
{
    float sum = 0.0f;
    for (size_t i = 0; i < sampleCount; i++) {
        sum += history[i];
    }
    rollingAverage = sum / static_cast<float>(sampleCount);
}

bool FrameTimeTracker::IsSpikeDetected(float frameTimeMs) const
{
    // Need at least 10 samples to detect spikes reliably
    if (sampleCount < 10) return false;

    return frameTimeMs > rollingAverage * spikeThreshold;
}

bool DllLoader::Load(const std::string& dllPath, const std::string& tempCopyName)
{
    originalPath = dllPath;

    if (!tempCopyName.empty()) {
        std::filesystem::path srcPath(dllPath);
        std::filesystem::path dstPath = srcPath.parent_path() / tempCopyName;

        std::error_code ec;
        std::filesystem::copy_file(srcPath, dstPath, std::filesystem::copy_options::overwrite_existing, ec);

        if (ec) {
            LOG_ERROR("Failed to copy DLL: {}", ec.message());
            return false;
        }

        loadedPath = dstPath.string();

        std::filesystem::path pdbSrc = srcPath;
        pdbSrc.replace_extension(".pdb");
        std::filesystem::path pdbDst = dstPath;
        pdbDst.replace_extension(".pdb");

        if (std::filesystem::exists(pdbSrc)) {
            std::filesystem::copy_file(pdbSrc, pdbDst, std::filesystem::copy_options::overwrite_existing, ec);
        }
    }
    else {
        loadedPath = dllPath;
    }

    handle = LoadLibraryA(loadedPath.c_str());
    if (!handle) {
        LOG_ERROR("Failed to load DLL: {}", loadedPath);
        return false;
    }

    LOG_INFO("Loaded DLL: {}", loadedPath);
    return true;
}

void DllLoader::Unload()
{
    if (handle) {
        FreeLibrary(handle);
        handle = nullptr;
        LOG_INFO("Unloaded DLL: {}", loadedPath);
    }
}

bool DllLoader::Reload()
{
    Unload();
    return Load(originalPath, loadedPath != originalPath ? std::filesystem::path(loadedPath).filename().string() : "");
}
} // Utils
