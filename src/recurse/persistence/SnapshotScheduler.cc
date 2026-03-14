#include "recurse/persistence/SnapshotScheduler.hh"

#include "fabric/core/Log.hh"
#include "fabric/platform/WriterQueue.hh"

namespace recurse {

SnapshotScheduler::SnapshotScheduler(WorldTransactionStore& txStore, fabric::platform::WriterQueue& writerQueue,
                                     DataProvider provider)
    : txStore_(txStore), writerQueue_(writerQueue), provider_(std::move(provider)) {}

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
    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> entries;
    for (const auto& coord : dirty_) {
        auto blob = provider_(coord.x, coord.y, coord.z);
        if (!blob.empty())
            entries.emplace_back(coord, std::move(blob));
    }
    dirty_.clear();
    if (entries.empty())
        return;
    int count = static_cast<int>(entries.size());
    writerQueue_.submit([this, entries = std::move(entries), count]() {
        for (const auto& [coord, blob] : entries)
            txStore_.saveSnapshot(coord.x, coord.y, coord.z, blob);
        FABRIC_LOG_INFO("Snapshot pass: saved {} chunks", count);
    });
}

} // namespace recurse
