#pragma once
#include "fabric/utils/ThreadPoolExecutor.hh"
#include "fabric/world/ChunkCoord.hh"
#include <functional>
#include <future>
#include <memory>
#include <vector>

namespace fabric {

/// Simulation-specific thread pool. Wraps ThreadPoolExecutor with chunk dispatch
/// and barrier synchronization for epoch-based simulation.
class SimulationThreadPool {
  public:
    /// threadCount defaults to hardware_concurrency() - 2, minimum 1
    explicit SimulationThreadPool(size_t threadCount = 0);
    ~SimulationThreadPool();

    /// Dispatch one task per chunk. Non-blocking; call barrierSync() after.
    void dispatchChunks(const std::vector<ChunkCoord>& chunks, const std::function<void(const ChunkCoord&)>& fn);

    /// Block until all dispatched chunk tasks complete.
    void barrierSync();

    size_t threadCount() const;

    /// For testing: run dispatched tasks inline (single-threaded, deterministic)
    void pauseForTesting();
    void resumeAfterTesting();

  private:
    std::unique_ptr<utils::ThreadPoolExecutor> pool_;
    std::vector<std::future<void>> futures_;
    size_t threadCount_;
};

} // namespace fabric
