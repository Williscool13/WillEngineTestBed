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

struct ClipPosition
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
 * [info] [Parallelization] (      10) Parallelized avg:          14 us (   0.014 ms)
 * [info] [Parallelization] (      10) Single-threaded avg:        6 us (   0.006 ms)
 * [info] [Parallelization] (     100) Parallelized avg:          18 us (   0.018 ms)
 * [info] [Parallelization] (     100) Single-threaded avg:       65 us (   0.065 ms)
 * [info] [Parallelization] (    1000) Parallelized avg:         103 us (   0.103 ms)
 * [info] [Parallelization] (    1000) Single-threaded avg:      673 us (   0.673 ms)
 * [info] [Parallelization] (   10000) Parallelized avg:         511 us (   0.511 ms)
 * [info] [Parallelization] (   10000) Single-threaded avg:     6860 us (   6.860 ms)
 * [info] [Parallelization] (  100000) Parallelized avg:        4497 us (   4.497 ms)
 * [info] [Parallelization] (  100000) Single-threaded avg:    65512 us (  65.512 ms)
 * [info] [Parallelization] ( 1000000) Parallelized avg:       42097 us (  42.097 ms)
 * [info] [Parallelization] ( 1000000) Single-threaded avg:   650287 us ( 650.287 ms)
 * [info] [Parallelization] (10000000) Parallelized avg:      426604 us ( 426.604 ms)
 * [info] [Parallelization] (10000000) Single-threaded avg:  6488630 us (6488.630 ms)
 */

/* alignas(64) on ClipPosition
 * [info] Scheduler operating with 32 threads.
 * [info] [Parallelization] (      10) Parallelized avg:          13 us (   0.013 ms)
 * [info] [Parallelization] (      10) Single-threaded avg:        9 us (   0.009 ms)
 * [info] [Parallelization] (     100) Parallelized avg:          14 us (   0.014 ms)
 * [info] [Parallelization] (     100) Single-threaded avg:       70 us (   0.070 ms)
 * [info] [Parallelization] (    1000) Parallelized avg:          99 us (   0.099 ms)
 * [info] [Parallelization] (    1000) Single-threaded avg:      693 us (   0.693 ms)
 * [info] [Parallelization] (   10000) Parallelized avg:         554 us (   0.554 ms)
 * [info] [Parallelization] (   10000) Single-threaded avg:     6663 us (   6.663 ms)
 * [info] [Parallelization] (  100000) Parallelized avg:        4423 us (   4.423 ms)
 * [info] [Parallelization] (  100000) Single-threaded avg:    66196 us (  66.196 ms)
 * [info] [Parallelization] ( 1000000) Parallelized avg:       42492 us (  42.492 ms)
 * [info] [Parallelization] ( 1000000) Single-threaded avg:   662021 us ( 662.021 ms)
 * [info] [Parallelization] (10000000) Parallelized avg:      426136 us ( 426.136 ms)
 * [info] [Parallelization] (10000000) Single-threaded avg:  6617584 us (6617.584 ms)
 */
