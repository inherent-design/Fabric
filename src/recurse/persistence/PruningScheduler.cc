#include "recurse/persistence/PruningScheduler.hh"

#include "fabric/log/Log.hh"
#include "fabric/platform/WriterQueue.hh"
#include <chrono>

namespace recurse {

PruningScheduler::PruningScheduler(WorldTransactionStore& txStore, fabric::platform::WriterQueue& writerQueue)
    : txStore_(txStore), writerQueue_(writerQueue) {}

void PruningScheduler::update(float dt) {
    elapsed_ += dt;
    if (elapsed_ >= intervalSeconds) {
        pruneNow();
        elapsed_ = 0.0f;
    }
}

void PruningScheduler::pruneNow() {
    auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    int64_t changeCutoff = now - changeRetentionMs;
    int64_t snapshotCutoff = now - snapshotRetentionMs;

    writerQueue_.submit([this, changeCutoff, snapshotCutoff]() {
        txStore_.prune(changeCutoff, snapshotCutoff);
        FABRIC_LOG_INFO("PruningScheduler: pruned changes before {}ms, snapshots before {}ms", changeCutoff,
                        snapshotCutoff);
    });
}

} // namespace recurse
