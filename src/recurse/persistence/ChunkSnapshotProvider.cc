#include "recurse/persistence/ChunkSnapshotProvider.hh"

#include "fabric/log/Log.hh"
#include "recurse/persistence/ReplayExecutor.hh"
#include "recurse/persistence/SnapshotScheduler.hh"
#include "recurse/persistence/WorldTransactionStore.hh"
#include "recurse/simulation/SimulationGrid.hh"

#include <chrono>

namespace recurse {

ChunkSnapshotProvider::ChunkSnapshotProvider(WorldTransactionStore& txStore, SnapshotScheduler& scheduler,
                                             simulation::SimulationGrid& grid,
                                             persistence::ReplayExecutor& replayExecutor)
    : txStore_(txStore), scheduler_(scheduler), grid_(grid), replayExecutor_(replayExecutor) {}

int64_t ChunkSnapshotProvider::onCreateSnapshot(double /*timelineTime*/) {
    scheduler_.flush();

    auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    return now;
}

void ChunkSnapshotProvider::onRestoreSnapshot(int64_t snapshotToken) {
    if (snapshotToken == 0) {
        FABRIC_LOG_WARN("ChunkSnapshotProvider: restore with null token, skipping");
        return;
    }

    auto allChunks = grid_.allChunks();

    persistence::SnapshotSet snapshots;
    snapshots.timeMs = snapshotToken;

    for (const auto& [cx, cy, cz] : allChunks) {
        auto blob = txStore_.loadSnapshot(cx, cy, cz, snapshotToken);
        if (blob.has_value()) {
            snapshots.chunks.push_back(persistence::ChunkSnapshot{fabric::ChunkCoord{cx, cy, cz}, std::move(*blob)});
        }
    }

    if (snapshots.chunks.empty()) {
        FABRIC_LOG_WARN("ChunkSnapshotProvider: no snapshots found for token {}", snapshotToken);
        return;
    }

    persistence::ReplayConfig config;
    config.headless = true;
    config.speed = 0.0f;

    replayExecutor_.replayDelta(snapshots, {}, 0, config, nullptr);

    FABRIC_LOG_INFO("ChunkSnapshotProvider: restored {} chunks from token {}", snapshots.chunks.size(), snapshotToken);
}

const char* ChunkSnapshotProvider::providerName() const {
    return "chunks";
}

} // namespace recurse
