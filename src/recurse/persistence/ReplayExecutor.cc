#include "recurse/persistence/ReplayExecutor.hh"

#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/WorldTransactionStore.hh"
#include "recurse/simulation/BoundaryWriteQueue.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "recurse/world/WorldGenerator.hh"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <random>
#include <unordered_set>

namespace recurse::persistence {

namespace {

constexpr double K_TICK_DURATION_MS = 1000.0 / 60.0;

} // namespace

ReplayExecutor::ReplayExecutor(WorldTransactionStore& txStore, simulation::SimulationGrid& grid,
                               simulation::FallingSandSystem& sandSystem, simulation::GhostCellManager& ghosts,
                               simulation::ChunkActivityTracker& tracker, int64_t worldSeed, WorldGenerator* worldGen)
    : txStore_(txStore),
      grid_(grid),
      sandSystem_(sandSystem),
      ghosts_(ghosts),
      tracker_(tracker),
      worldSeed_(worldSeed),
      worldGen_(worldGen) {}

ReplayResult ReplayExecutor::replayDelta(const SnapshotSet& snapshot, std::span<const VoxelChange> userEdits,
                                         uint64_t tickCount, ReplayConfig config, ReplayObserver observer) {
    return runLoop(snapshot, userEdits, tickCount, config, std::move(observer));
}

ReplayResult ReplayExecutor::replayToTime(const SnapshotSet& snapshot, std::span<const VoxelChange> userEdits,
                                          int64_t targetTimeMs, ReplayConfig config, ReplayObserver observer) {
    if (targetTimeMs <= snapshot.timeMs) {
        return ReplayResult{ReplayStatus::Ok, 0, snapshot.timeMs, {}};
    }
    auto tickCount = static_cast<uint64_t>(static_cast<double>(targetTimeMs - snapshot.timeMs) / K_TICK_DURATION_MS);
    return runLoop(snapshot, userEdits, tickCount, config, std::move(observer));
}

ReplayResult ReplayExecutor::runLoop(const SnapshotSet& snapshot, std::span<const VoxelChange> userEdits,
                                     uint64_t tickCount, ReplayConfig config, ReplayObserver observer) {
    using namespace recurse::simulation;

    if (snapshot.chunks.empty()) {
        return ReplayResult{ReplayStatus::SnapshotMissing, 0, 0, {}};
    }

    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> affectedSet;

    // Phase 0: Load snapshot into grid.
    for (const auto& cs : snapshot.chunks) {
        FchkDecoded decoded;
        if (FchkCodec::isDelta(cs.blob) && worldGen_) {
            auto refBuf = std::make_unique<std::array<VoxelCell, K_CHUNK_VOLUME>>();
            worldGen_->generateToBuffer(refBuf->data(), cs.coord.x, cs.coord.y, cs.coord.z);
            decoded = FchkCodec::decodeAny(cs.blob, refBuf->data());
        } else {
            decoded = FchkCodec::decodeAny(cs.blob);
        }
        if (decoded.cells.empty()) {
            return ReplayResult{ReplayStatus::SnapshotDecodeFailed, 0, 0, {}};
        }

        grid_.materializeChunk(cs.coord.x, cs.coord.y, cs.coord.z);

        auto* writeBuf = grid_.writeBuffer(cs.coord.x, cs.coord.y, cs.coord.z);
        if (writeBuf) {
            std::memcpy(writeBuf->data(), decoded.cells.data(),
                        std::min(decoded.cells.size(), static_cast<size_t>(K_CHUNK_VOLUME) * sizeof(VoxelCell)));
        }

        grid_.syncChunkBuffers(cs.coord.x, cs.coord.y, cs.coord.z);

        tracker_.setState(cs.coord, ChunkState::Active);
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += K_PHYS_TILE_SIZE)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += K_PHYS_TILE_SIZE)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += K_PHYS_TILE_SIZE)
                    tracker_.markSubRegionActive(cs.coord, lx, ly, lz);
    }

    // Pre-sort user edits by tick index (ascending).
    struct TickEdit {
        uint64_t tick;
        const VoxelChange* edit;
    };
    std::vector<TickEdit> sortedEdits;
    sortedEdits.reserve(userEdits.size());

    for (const auto& edit : userEdits) {
        if (edit.source != ChangeSource::Place && edit.source != ChangeSource::Destroy)
            continue;
        auto tickIdx = static_cast<uint64_t>(
            std::max(0.0, static_cast<double>(edit.timestamp - snapshot.timeMs) / K_TICK_DURATION_MS));
        sortedEdits.push_back({tickIdx, &edit});
    }

    std::sort(sortedEdits.begin(), sortedEdits.end(),
              [](const TickEdit& a, const TickEdit& b) { return a.tick < b.tick; });

    size_t editCursor = 0;

    // Tick loop.
    for (uint64_t tick = 0; tick < tickCount; ++tick) {

        // Step 1: Apply user edits at this tick boundary.
        while (editCursor < sortedEdits.size() && sortedEdits[editCursor].tick <= tick) {
            const auto& edit = *sortedEdits[editCursor].edit;
            int wx = edit.addr.cx * K_CHUNK_SIZE + edit.addr.vx;
            int wy = edit.addr.cy * K_CHUNK_SIZE + edit.addr.vy;
            int wz = edit.addr.cz * K_CHUNK_SIZE + edit.addr.vz;

            VoxelCell cell{};
            std::memcpy(&cell, &edit.newCell, sizeof(cell));
            grid_.writeCell(wx, wy, wz, cell);

            fabric::ChunkCoord editChunk{edit.addr.cx, edit.addr.cy, edit.addr.cz};
            if (tracker_.getState(editChunk) == ChunkState::Sleeping) {
                tracker_.setState(editChunk, ChunkState::Active);
                for (int lz = 0; lz < K_CHUNK_SIZE; lz += K_PHYS_TILE_SIZE)
                    for (int ly = 0; ly < K_CHUNK_SIZE; ly += K_PHYS_TILE_SIZE)
                        for (int lx = 0; lx < K_CHUNK_SIZE; lx += K_PHYS_TILE_SIZE)
                            tracker_.markSubRegionActive(editChunk, lx, ly, lz);
            }
            affectedSet.insert(editChunk);
            ++editCursor;
        }

        // Step 2: Collect active chunks.
        auto collected = tracker_.collectActiveChunks();
        std::vector<ActiveChunkEntry> active;
        active.reserve(collected.size());
        for (const auto& entry : collected) {
            if (tracker_.getState(entry.pos) == ChunkState::Active)
                active.push_back(entry);
        }

        if (active.empty()) {
            // Still call observer for frame-accurate visual replay.
            if (!config.headless && observer) {
                ReplayFrame frame{};
                frame.tick = tick;
                frame.worldTimeMs =
                    snapshot.timeMs + static_cast<int64_t>(static_cast<double>(tick) * K_TICK_DURATION_MS);
                frame.grid = &grid_;
                if (!observer(frame)) {
                    ReplayResult result{};
                    result.status = ReplayStatus::Aborted;
                    result.ticksReplayed = tick + 1;
                    result.finalTimeMs = frame.worldTimeMs;
                    result.affectedChunks.assign(affectedSet.begin(), affectedSet.end());
                    return result;
                }
            }
            continue;
        }

        // Sort active set by coordinate for deterministic iteration.
        std::sort(active.begin(), active.end(),
                  [](const ActiveChunkEntry& a, const ActiveChunkEntry& b) { return a.pos < b.pos; });

        // Step 3: Resolve buffer pointers.
        grid_.registry().resolveBufferPointers(grid_.currentEpoch());

        // Step 4: Sync ghost cells.
        std::vector<ChunkCoord> positions;
        positions.reserve(active.size());
        for (const auto& entry : active)
            positions.push_back(entry.pos);
        ghosts_.syncAll(positions, grid_);

        // Step 5: Pre-materialize face neighbors.
        static constexpr int offsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const auto& entry : active) {
            const auto& p = entry.pos;
            for (const auto& off : offsets) {
                int nx = p.x + off[0], ny = p.y + off[1], nz = p.z + off[2];
                if (grid_.hasChunk(nx, ny, nz))
                    grid_.materializeChunk(nx, ny, nz);
            }
        }

        // Step 6: Simulate all chunks SEQUENTIALLY (deterministic).
        std::vector<BoundaryWrite> allBoundaryWrites;
        std::vector<CellSwap> discardedSwaps;
        for (const auto& entry : active) {
            const auto& pos = entry.pos;
            uint64_t hash = spatialHash(pos);

            std::mt19937 rng(static_cast<uint32_t>(worldSeed_ ^ hash));
            bool reverseDir = (hash & 1) != 0;

            BoundaryWriteQueue boundaryWrites;
            discardedSwaps.clear();

            sandSystem_.simulateChunk(pos, grid_, ghosts_, tracker_, reverseDir, rng, boundaryWrites, discardedSwaps);

            allBoundaryWrites.insert(allBoundaryWrites.end(), boundaryWrites.begin(), boundaryWrites.end());
            affectedSet.insert(pos);
        }

        // Step 7: Drain boundary writes (sorted, with conflict conservation).
        std::sort(allBoundaryWrites.begin(), allBoundaryWrites.end(),
                  [](const BoundaryWrite& a, const BoundaryWrite& b) {
                      if (a.dstWx != b.dstWx)
                          return a.dstWx < b.dstWx;
                      if (a.dstWy != b.dstWy)
                          return a.dstWy < b.dstWy;
                      if (a.dstWz != b.dstWz)
                          return a.dstWz < b.dstWz;
                      if (a.srcWx != b.srcWx)
                          return a.srcWx < b.srcWx;
                      if (a.srcWy != b.srcWy)
                          return a.srcWy < b.srcWy;
                      return a.srcWz < b.srcWz;
                  });

        struct WorldCoordHash {
            size_t operator()(const std::tuple<int, int, int>& c) const {
                size_t h = std::hash<int>{}(std::get<0>(c));
                h ^= std::hash<int>{}(std::get<1>(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<int>{}(std::get<2>(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                return h;
            }
        };
        std::unordered_set<std::tuple<int, int, int>, WorldCoordHash> writtenDests;

        for (const auto& bw : allBoundaryWrites) {
            auto dest = std::make_tuple(bw.dstWx, bw.dstWy, bw.dstWz);
            if (writtenDests.count(dest)) {
                grid_.writeCell(bw.srcWx, bw.srcWy, bw.srcWz, bw.undoCell);
            } else {
                if (grid_.writeCellIfExists(bw.dstWx, bw.dstWy, bw.dstWz, bw.writeCell)) {
                    writtenDests.insert(dest);
                    tracker_.notifyBoundaryChange(bw.neighborChunk);
                } else {
                    grid_.writeCell(bw.srcWx, bw.srcWy, bw.srcWz, bw.undoCell);
                }
            }
        }

        // Step 8: Advance epoch.
        grid_.advanceEpoch();

        // Step 9: Propagate dirty (wake neighbors of active chunks).
        for (const auto& entry : active) {
            if (tracker_.getState(entry.pos) == ChunkState::Active) {
                const auto& p = entry.pos;
                for (const auto& off : offsets)
                    tracker_.notifyBoundaryChange(fabric::ChunkCoord{p.x + off[0], p.y + off[1], p.z + off[2]});
            }
        }

        // Step 10: Visual callback.
        if (!config.headless && observer) {
            ReplayFrame frame{};
            frame.tick = tick;
            frame.worldTimeMs = snapshot.timeMs + static_cast<int64_t>(static_cast<double>(tick) * K_TICK_DURATION_MS);
            frame.grid = &grid_;
            if (!observer(frame)) {
                ReplayResult result{};
                result.status = ReplayStatus::Aborted;
                result.ticksReplayed = tick + 1;
                result.finalTimeMs = frame.worldTimeMs;
                result.affectedChunks.assign(affectedSet.begin(), affectedSet.end());
                return result;
            }
        }
    }

    ReplayResult result{};
    result.status = ReplayStatus::Ok;
    result.ticksReplayed = tickCount;
    result.finalTimeMs = snapshot.timeMs + static_cast<int64_t>(static_cast<double>(tickCount) * K_TICK_DURATION_MS);
    result.affectedChunks.assign(affectedSet.begin(), affectedSet.end());
    return result;
}

} // namespace recurse::persistence
