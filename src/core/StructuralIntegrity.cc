#include "fabric/core/StructuralIntegrity.hh"

#include "fabric/core/ChunkedGrid.hh"

#include <chrono>

namespace fabric {

StructuralIntegrity::StructuralIntegrity() = default;

void StructuralIntegrity::setDebrisCallback(DebrisCallback cb) {
    debrisCallback_ = std::move(cb);
}

void StructuralIntegrity::setPerFrameBudgetMs(float budgetMs) {
    perFrameBudgetMs_ = budgetMs;
}

float StructuralIntegrity::getPerFrameBudgetMs() const {
    return perFrameBudgetMs_;
}

const StructuralIntegrity::FloodFillState* StructuralIntegrity::getPartialState(int64_t chunkKey) const {
    auto it = partialStates_.find(chunkKey);
    if (it == partialStates_.end()) {
        return nullptr;
    }
    return &it->second;
}

void StructuralIntegrity::update(const ChunkedGrid<float>& grid, float dt) {
    (void)dt;

    if (perFrameBudgetMs_ <= 0.0f || !debrisCallback_) {
        return;
    }

    const auto active = grid.activeChunks();
    if (active.empty()) {
        checkedChunks_.clear();
        partialStates_.clear();
        return;
    }

    std::unordered_set<int64_t> activeKeys;
    activeKeys.reserve(active.size());
    for (const auto& [cx, cy, cz] : active) {
        activeKeys.insert(packKey(cx, cy, cz));
    }

    for (auto it = checkedChunks_.begin(); it != checkedChunks_.end();) {
        if (activeKeys.count(it->first) == 0) {
            it = checkedChunks_.erase(it);
        } else {
            ++it;
        }
    }

    // Evict partial states for chunks that are no longer active
    for (auto it = partialStates_.begin(); it != partialStates_.end();) {
        if (activeKeys.count(it->first) == 0) {
            it = partialStates_.erase(it);
        } else {
            ++it;
        }
    }

    const auto frameStart = std::chrono::high_resolution_clock::now();
    const int64_t totalBudgetNs = static_cast<int64_t>(perFrameBudgetMs_ * 1'000'000.0f);

    // Resume any in-progress partial states first
    for (auto it = partialStates_.begin(); it != partialStates_.end();) {
        const auto elapsed = std::chrono::high_resolution_clock::now() - frameStart;
        const int64_t elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        if (elapsedNs >= totalBudgetNs) {
            return;
        }

        const int64_t chunkKey = it->first;
        int cx = 0;
        int cy = 0;
        int cz = 0;
        unpackKey(chunkKey, cx, cy, cz);

        const int64_t remainingNs = totalBudgetNs - elapsedNs;
        bool complete = floodFillChunk(cx, cy, cz, grid, it->second, remainingNs);

        if (complete) {
            processFloodFillResults(grid, it->second);
            checkedChunks_[chunkKey] = 1;
            it = partialStates_.erase(it);
        } else {
            // Budget exhausted mid-BFS; stop processing this frame
            return;
        }
    }

    // Process new chunks
    for (const auto& [cx, cy, cz] : active) {
        const int64_t chunkKey = packKey(cx, cy, cz);
        if (checkedChunks_.count(chunkKey) != 0) {
            continue;
        }
        // Skip if we already have partial state (shouldn't happen since we process those above,
        // but guard against it)
        if (partialStates_.count(chunkKey) != 0) {
            continue;
        }

        const auto elapsed = std::chrono::high_resolution_clock::now() - frameStart;
        const int64_t elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        if (elapsedNs >= totalBudgetNs) {
            break;
        }

        FloodFillState state;
        const int64_t remainingNs = totalBudgetNs - elapsedNs;
        bool complete = floodFillChunk(cx, cy, cz, grid, state, remainingNs);

        if (complete) {
            processFloodFillResults(grid, state);
            checkedChunks_[chunkKey] = 1;
        } else {
            // Stash partial state for resumption next frame
            partialStates_[chunkKey] = std::move(state);
            break;
        }
    }

    if (checkedChunks_.size() >= activeKeys.size() && partialStates_.empty()) {
        checkedChunks_.clear();
    }
}

bool StructuralIntegrity::floodFillChunk(int cx, int cy, int cz, const ChunkedGrid<float>& grid, FloodFillState& state,
                                         int64_t timeBudgetNs) {
    constexpr float kDensityThreshold = 0.5f;
    constexpr int kNeighborOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    const int baseX = cx * kStructuralIntegrityChunkSize;
    const int baseY = cy * kStructuralIntegrityChunkSize;
    const int baseZ = cz * kStructuralIntegrityChunkSize;

    const auto startTime = std::chrono::high_resolution_clock::now();

    auto budgetExhausted = [&]() -> bool {
        const auto elapsed = std::chrono::high_resolution_clock::now() - startTime;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() >= timeBudgetNs;
    };

    // Phase 1: Enumerate dense voxels (only on first entry)
    if (state.phase == FloodFillState::Phase::EnumerateVoxels) {
        for (int lz = 0; lz < kStructuralIntegrityChunkSize; ++lz) {
            for (int ly = 0; ly < kStructuralIntegrityChunkSize; ++ly) {
                for (int lx = 0; lx < kStructuralIntegrityChunkSize; ++lx) {
                    const int wx = baseX + lx;
                    const int wy = baseY + ly;
                    const int wz = baseZ + lz;

                    if (grid.get(wx, wy, wz) >= kDensityThreshold) {
                        state.denseVoxels.push_back({wx, wy, wz});
                    }
                }
            }
        }

        if (state.denseVoxels.empty()) {
            state.phase = FloodFillState::Phase::Done;
            return true;
        }

        state.phase = FloodFillState::Phase::SeedGround;
    }

    // Phase 2: Seed ground-connected voxels
    if (state.phase == FloodFillState::Phase::SeedGround) {
        for (const auto& voxel : state.denseVoxels) {
            if (voxel[1] <= 0) {
                const int64_t key = packKey(voxel[0], voxel[1], voxel[2]);
                if (state.supported.insert(key).second) {
                    state.queue.push(voxel);
                }
            }
        }

        state.phase = FloodFillState::Phase::BFS;
    }

    // Phase 3: BFS expansion (interruptible)
    if (state.phase == FloodFillState::Phase::BFS) {
        int iterationsSinceCheck = 0;

        while (!state.queue.empty()) {
            const auto current = state.queue.front();
            state.queue.pop();
            ++state.processedCells;
            ++iterationsSinceCheck;

            for (const auto& off : kNeighborOffsets) {
                const int nx = current[0] + off[0];
                const int ny = current[1] + off[1];
                const int nz = current[2] + off[2];

                if (grid.get(nx, ny, nz) < kDensityThreshold) {
                    continue;
                }

                const int64_t nkey = packKey(nx, ny, nz);
                if (state.supported.insert(nkey).second) {
                    state.queue.push({nx, ny, nz});
                }
            }

            if (iterationsSinceCheck >= kTimingCheckInterval) {
                iterationsSinceCheck = 0;
                if (budgetExhausted()) {
                    return false;
                }
            }
        }

        state.phase = FloodFillState::Phase::CollectUnsupported;
    }

    // Phase 4: Collect unsupported voxels
    if (state.phase == FloodFillState::Phase::CollectUnsupported) {
        for (const auto& voxel : state.denseVoxels) {
            const int64_t key = packKey(voxel[0], voxel[1], voxel[2]);
            if (state.supported.count(key) == 0) {
                state.disconnectedVoxels.push_back(voxel);
            }
        }

        state.phase = FloodFillState::Phase::Done;
    }

    return true;
}

void StructuralIntegrity::processFloodFillResults(const ChunkedGrid<float>& grid, FloodFillState& state) {
    if (!debrisCallback_) {
        return;
    }

    for (const auto& voxel : state.disconnectedVoxels) {
        const int wx = voxel[0];
        const int wy = voxel[1];
        const int wz = voxel[2];

        const float density = grid.get(wx, wy, wz);
        if (density > 0.0f) {
            debrisCallback_(DebrisEvent{wx, wy, wz, density});
        }
    }
}

} // namespace fabric
