#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"
#include <mutex>
#include <TaskScheduler.h>
#include <thread>

namespace fabric {

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
    FABRIC_ZONE_SCOPED_N("job_scheduler_dispatch");

    if (count == 0)
        return;

    if (disabled_ || count == 1 || workerCount_ <= 1) {
        runInline(count, fn);
        return;
    }

    enki::TaskSet taskSet(static_cast<uint32_t>(count), [&fn](enki::TaskSetPartition range, uint32_t threadnum) {
        for (uint32_t i = range.start; i < range.end; ++i)
            fn(static_cast<size_t>(i), static_cast<size_t>(threadnum));
    });
    scheduler_->AddTaskSetToPipe(&taskSet);
    scheduler_->WaitforTask(&taskSet);
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

} // namespace fabric
