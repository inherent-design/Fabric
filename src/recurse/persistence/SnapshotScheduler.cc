#include "recurse/persistence/SnapshotScheduler.hh"

#include "fabric/core/Log.hh"

namespace recurse {

SnapshotScheduler::SnapshotScheduler(WorldTransactionStore& txStore, DataProvider provider)
    : txStore_(txStore), provider_(std::move(provider)) {}

void SnapshotScheduler::markDirty(int cx, int cy, int cz) {
    dirty_.insert(fabric::ChunkCoord{cx, cy, cz});
}

void SnapshotScheduler::update(float dt) {
    elapsed_ += dt;
    if (elapsed_ >= intervalSeconds && !dirty_.empty()) {
        snapshotAll();
        elapsed_ = 0.0f;
    }
}

void SnapshotScheduler::flush() {
    if (!dirty_.empty())
        snapshotAll();
    elapsed_ = 0.0f;
}

size_t SnapshotScheduler::pendingCount() const {
    return dirty_.size();
}

void SnapshotScheduler::snapshotAll() {
    int count = 0;
    for (const auto& coord : dirty_) {
        auto blob = provider_(coord.x, coord.y, coord.z);
        if (!blob.empty()) {
            txStore_.saveSnapshot(coord.x, coord.y, coord.z, blob);
            ++count;
        }
    }
    dirty_.clear();
    FABRIC_LOG_INFO("Snapshot pass: saved {} chunks", count);
}

} // namespace recurse
