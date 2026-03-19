#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"
#include <cstdint>
#include <mutex>
#include <TaskScheduler.h>
#include <thread>

namespace fabric {

#define FABRIC_ANNOTATE_PARALLEL_DISPATCH(traceLabel, workUnits)                                                       \
    do {                                                                                                               \
        const auto& fabricTraceLabel_ = (traceLabel);                                                                  \
        const auto fabricWorkUnits_ = (workUnits);                                                                     \
        if (!fabricTraceLabel_.empty()) {                                                                              \
            FABRIC_ZONE_TEXT(fabricTraceLabel_.data(), fabricTraceLabel_.size());                                      \
        }                                                                                                              \
        FABRIC_ZONE_VALUE(fabricWorkUnits_);                                                                           \
    } while (false)

struct JobScheduler::PendingTask {
    enki::TaskSet task;
    PendingTask(uint32_t size, std::function<void(enki::TaskSetPartition, uint32_t)> func)
        : task(size, std::move(func)) {}
};

JobScheduler::JobScheduler(size_t threadCount) {
    if (threadCount == 0) {
        auto hw = std::thread::hardware_concurrency();
        workerCount_ = (hw > 2) ? (hw - 2) : 1;
    } else {
        workerCount_ = threadCount;
    }

    scheduler_ = std::make_unique<enki::TaskScheduler>();
    enki::TaskSchedulerConfig config;
    config.numTaskThreadsToCreate = workerCount_;
    scheduler_->Initialize(config);
}

JobScheduler::~JobScheduler() {
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        for (auto& p : pendingTasks_)
            scheduler_->WaitforTask(&p->task);
        pendingTasks_.clear();
    }
    scheduler_->WaitforAllAndShutdown();
}

void JobScheduler::disableForTesting() {
    disabled_ = true;
}

size_t JobScheduler::workerCount() const {
    return workerCount_;
}

ConcurrencyDebugInfo JobScheduler::debugInfo() const {
    ConcurrencyDebugInfo info;
    info.activeWorkers = static_cast<int>(workerCount_);
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        info.queuedJobs = static_cast<int>(pendingTasks_.size());
    }
    return info;
}

void JobScheduler::runInline(size_t count, const std::function<void(size_t jobIdx, size_t workerIdx)>& fn) {
    for (size_t i = 0; i < count; ++i)
        fn(i, 0);
}

void JobScheduler::parallelFor(size_t count, std::function<void(size_t jobIdx, size_t workerIdx)> fn) {
    parallelFor(count, std::string_view{}, std::move(fn));
}

void JobScheduler::parallelFor(size_t count, std::string_view traceLabel,
                               std::function<void(size_t jobIdx, size_t workerIdx)> fn) {
    FABRIC_ZONE_SCOPED_N("job_scheduler_dispatch");
    FABRIC_ANNOTATE_PARALLEL_DISPATCH(traceLabel, static_cast<int64_t>(count));

    if (count == 0)
        return;

    if (disabled_ || count == 1 || workerCount_ <= 1) {
        FABRIC_ZONE_SCOPED_N("job_scheduler_inline_execute");
        FABRIC_ANNOTATE_PARALLEL_DISPATCH(traceLabel, static_cast<int64_t>(count));
        runInline(count, fn);
        return;
    }

    enki::TaskSet taskSet(static_cast<uint32_t>(count),
                          [&fn, traceLabel](enki::TaskSetPartition range, uint32_t threadnum) {
                              const auto partitionSize = static_cast<int64_t>(range.end - range.start);
                              if (threadnum == 0) {
                                  FABRIC_ZONE_SCOPED_N("job_scheduler_caller_execute");
                                  FABRIC_ANNOTATE_PARALLEL_DISPATCH(traceLabel, partitionSize);
                                  for (uint32_t i = range.start; i < range.end; ++i)
                                      fn(static_cast<size_t>(i), static_cast<size_t>(threadnum));
                                  return;
                              }

                              FABRIC_ZONE_SCOPED_N("job_scheduler_worker_execute");
                              FABRIC_ANNOTATE_PARALLEL_DISPATCH(traceLabel, partitionSize);
                              for (uint32_t i = range.start; i < range.end; ++i)
                                  fn(static_cast<size_t>(i), static_cast<size_t>(threadnum));
                          });

    {
        FABRIC_ZONE_SCOPED_N("job_scheduler_enqueue");
        FABRIC_ANNOTATE_PARALLEL_DISPATCH(traceLabel, static_cast<int64_t>(count));
        scheduler_->AddTaskSetToPipe(&taskSet);
    }
    {
        FABRIC_ZONE_SCOPED_N("job_scheduler_wait");
        FABRIC_ANNOTATE_PARALLEL_DISPATCH(traceLabel, static_cast<int64_t>(count));
        scheduler_->WaitforTask(&taskSet);
    }
}

void JobScheduler::submitAsync(std::function<void()> work, bool background) {
    FABRIC_ZONE_SCOPED_N("job_scheduler_submit_async");

    auto pending = std::make_unique<PendingTask>(1, [w = std::move(work)](enki::TaskSetPartition, uint32_t) { w(); });

    if (background)
        pending->task.m_Priority = enki::TASK_PRIORITY_LOW;

    auto* rawPtr = &pending->task;
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingTasks_.push_back(std::move(pending));
    }
    scheduler_->AddTaskSetToPipe(rawPtr);
}

void JobScheduler::submitBackground(std::function<void()> fn) {
    if (disabled_) {
        fn();
        return;
    }
    submitAsync(std::move(fn), true);
}

#undef FABRIC_ANNOTATE_PARALLEL_DISPATCH

} // namespace fabric
