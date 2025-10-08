#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glm/gtc/quaternion.hpp"
#include <enkiTS/src/TaskScheduler.h>

#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

#include "types.h"


struct Position
{
    glm::vec3 position{0.0f};
};

struct Model
{
    glm::mat4 model{1.0f};
};

struct alignas(64) ClipPosition
{
    glm::vec4 clipPos{0.0f, 0.0f, 0.0f, 1.0f};
};

struct CameraState
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
};

struct ParallelTransformTask final : enki::ITaskSet
{
    std::vector<Position>* positions;
    Model* model;
    CameraState* cameraState;
    std::vector<ClipPosition>* clips;

    // info: adjust m_MinRange for low count to reduce overhead cost of task creation.
    ParallelTransformTask(std::vector<Position>* positions, Model* model, CameraState* camera, std::vector<ClipPosition>* clips, uint32_t count)
        : positions(positions), model(model), cameraState(camera), clips(clips)
    {
        m_SetSize = count;
    }

    void ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_) override
    {
        for (uint32_t i = range_.start; i < range_.end; ++i) {
            auto pos = glm::vec4((*positions)[i].position, 1.0f);
            (*clips)[i].clipPos = cameraState->viewProj * model->model * pos;
        }
    }
};

void Benchmark(enki::TaskScheduler& scheduler, int32_t count, int32_t iterations)
{
    std::vector<Position> positions(count);

    for (size_t i = 0; i < count; ++i) {
        auto v = static_cast<float>(i);
        positions[i].position = glm::vec3(v * 0.1f, v * 0.2f, v * 0.3f);
    }

    glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
    glm::mat4 viewMatrix = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 projMatrix = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    Model model{worldMatrix};
    CameraState cameraState{viewMatrix, projMatrix, projMatrix * viewMatrix};
    std::vector<ClipPosition> clips(count);

    // Parallel
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int iter = 0; iter < iterations; ++iter) {
            ParallelTransformTask task(&positions, &model, &cameraState, &clips, count);
            scheduler.AddTaskSetToPipe(&task);
            scheduler.WaitforTask(&task);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        auto average = duration.count() / iterations;

        LOG_INFO("[Parallelization] ({:>8}) Parallelized avg:    {:>8} us ({:>8.3f} ms)",count, average, average / 1000.0);
    }

    // Single-threaded
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int iter = 0; iter < iterations; ++iter) {
            for (size_t i = 0; i < count; ++i) {
                auto pos = glm::vec4(positions[i].position, 1.0f);
                clips[i].clipPos = cameraState.viewProj * model.model * pos;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        auto average = duration.count() / iterations;

        LOG_INFO("[Parallelization] ({:>8}) Single-threaded avg: {:>8} us ({:>8.3f} ms)", count, average, average / 1000.0);
    }
}
int main()
{
    fmt::println("=== Parallelization ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/enkiTS.log");

    enki::TaskScheduler scheduler;
    enki::TaskSchedulerConfig config;
    config.numTaskThreadsToCreate = enki::GetNumHardwareThreads() - 1;
    LOG_INFO("Scheduler operating with {} threads.", config.numTaskThreadsToCreate + 1);
    scheduler.Initialize(config);

    Benchmark(scheduler, 10, 1000);
    Benchmark(scheduler, 100, 100);
    Benchmark(scheduler, 1000, 100);
    Benchmark(scheduler, 10000, 10);
    Benchmark(scheduler, 100000, 10);
    Benchmark(scheduler, 1000000, 10);
    Benchmark(scheduler, 10000000, 10);

    scheduler.WaitforAllAndShutdown();

    return 0;
}

/*
 * [info] [Parallelization] (      10) Parallelized avg:           6 us (   0.006 ms)
 * [info] [Parallelization] (      10) Single-threaded avg:        0 us (   0.000 ms)
 * [info] [Parallelization] (     100) Parallelized avg:          23 us (   0.023 ms)
 * [info] [Parallelization] (     100) Single-threaded avg:        1 us (   0.001 ms)
 * [info] [Parallelization] (    1000) Parallelized avg:          81 us (   0.081 ms)
 * [info] [Parallelization] (    1000) Single-threaded avg:       13 us (   0.013 ms)
 * [info] [Parallelization] (   10000) Parallelized avg:         117 us (   0.117 ms)
 * [info] [Parallelization] (   10000) Single-threaded avg:      143 us (   0.143 ms)
 * [info] [Parallelization] (  100000) Parallelized avg:         300 us (   0.300 ms)
 * [info] [Parallelization] (  100000) Single-threaded avg:     1475 us (   1.475 ms)
 * [info] [Parallelization] ( 1000000) Parallelized avg:        1313 us (   1.313 ms)
 * [info] [Parallelization] ( 1000000) Single-threaded avg:    14459 us (  14.459 ms)
 * [info] [Parallelization] (10000000) Parallelized avg:        9757 us (   9.757 ms)
 * [info] [Parallelization] (10000000) Single-threaded avg:   137208 us ( 137.208 ms)
 */

/* alignas(64) on ClipPosition
 * [info] Scheduler operating with 32 threads.
 * [info] [Parallelization] (      10) Parallelized avg:           5 us (   0.005 ms)
 * [info] [Parallelization] (      10) Single-threaded avg:        0 us (   0.000 ms)
 * [info] [Parallelization] (     100) Parallelized avg:          19 us (   0.019 ms)
 * [info] [Parallelization] (     100) Single-threaded avg:        1 us (   0.001 ms)
 * [info] [Parallelization] (    1000) Parallelized avg:          80 us (   0.080 ms)
 * [info] [Parallelization] (    1000) Single-threaded avg:       13 us (   0.013 ms)
 * [info] [Parallelization] (   10000) Parallelized avg:         121 us (   0.121 ms)
 * [info] [Parallelization] (   10000) Single-threaded avg:      152 us (   0.152 ms)
 * [info] [Parallelization] (  100000) Parallelized avg:         320 us (   0.320 ms)
 * [info] [Parallelization] (  100000) Single-threaded avg:     1622 us (   1.622 ms)
 * [info] [Parallelization] ( 1000000) Parallelized avg:        1842 us (   1.842 ms)
 * [info] [Parallelization] ( 1000000) Single-threaded avg:    14985 us (  14.985 ms)
 * [info] [Parallelization] (10000000) Parallelized avg:       26127 us (  26.127 ms)
 * [info] [Parallelization] (10000000) Single-threaded avg:   135836 us ( 135.836 ms)
 */
