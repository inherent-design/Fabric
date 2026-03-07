#include "fabric/core/SimulationThreadPool.hh"
#include <algorithm>
#include <thread>

namespace fabric {

SimulationThreadPool::SimulationThreadPool(size_t threadCount) {
    if (threadCount == 0) {
        auto hw = std::thread::hardware_concurrency();
        threadCount_ = std::max(static_cast<size_t>(1), static_cast<size_t>(hw > 2 ? hw - 2 : 1));
    } else {
        threadCount_ = threadCount;
    }
    pool_ = std::make_unique<utils::ThreadPoolExecutor>(threadCount_);
}

SimulationThreadPool::~SimulationThreadPool() {
    if (pool_) {
        pool_->shutdown();
    }
}

void SimulationThreadPool::dispatchChunks(const std::vector<ChunkCoord>& chunks,
                                          const std::function<void(const ChunkCoord&)>& fn) {
    futures_.reserve(futures_.size() + chunks.size());
    for (const auto& chunk : chunks) {
        futures_.push_back(pool_->submit([fn, chunk]() { fn(chunk); }));
    }
}

void SimulationThreadPool::barrierSync() {
    for (auto& f : futures_) {
        f.get();
    }
    futures_.clear();
}

size_t SimulationThreadPool::threadCount() const {
    return threadCount_;
}

void SimulationThreadPool::pauseForTesting() {
    pool_->pauseForTesting();
}

void SimulationThreadPool::resumeAfterTesting() {
    pool_->resumeAfterTesting();
}

} // namespace fabric
