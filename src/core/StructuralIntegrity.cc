#include "fabric/core/StructuralIntegrity.hh"

#include "fabric/core/ChunkedGrid.hh"

#include <chrono>
#include <queue>
#include <unordered_set>

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

void StructuralIntegrity::update(const ChunkedGrid<float>& grid, float dt) {
    (void)dt;

    if (perFrameBudgetMs_ <= 0.0f || !debrisCallback_) {
        return;
    }

    const auto active = grid.activeChunks();
    if (active.empty()) {
        checkedChunks_.clear();
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

    const auto frameStart = std::chrono::high_resolution_clock::now();
    const int64_t totalBudgetNs = static_cast<int64_t>(perFrameBudgetMs_ * 1'000'000.0f);

    for (const auto& [cx, cy, cz] : active) {
        const int64_t chunkKey = packKey(cx, cy, cz);
        if (checkedChunks_.count(chunkKey) != 0) {
            continue;
        }

        const auto elapsed = std::chrono::high_resolution_clock::now() - frameStart;
        const int64_t elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        if (elapsedNs >= totalBudgetNs) {
            break;
        }

        FloodFillState state;
        const int64_t remainingNs = totalBudgetNs - elapsedNs;
        floodFillChunk(cx, cy, cz, grid, state, static_cast<float>(remainingNs));
        processFloodFillResults(grid, state);
        checkedChunks_[chunkKey] = 1;
    }

    if (checkedChunks_.size() >= activeKeys.size()) {
        checkedChunks_.clear();
    }
}

bool StructuralIntegrity::floodFillChunk(int cx, int cy, int cz, const ChunkedGrid<float>& grid, FloodFillState& state,
                                         float timeBudgetNs) {
    (void)timeBudgetNs;

    constexpr float kDensityThreshold = 0.5f;
    constexpr int kNeighborOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    const int baseX = cx * kStructuralIntegrityChunkSize;
    const int baseY = cy * kStructuralIntegrityChunkSize;
    const int baseZ = cz * kStructuralIntegrityChunkSize;

    std::vector<std::array<int, 3>> denseVoxels;

    for (int lz = 0; lz < kStructuralIntegrityChunkSize; ++lz) {
        for (int ly = 0; ly < kStructuralIntegrityChunkSize; ++ly) {
            for (int lx = 0; lx < kStructuralIntegrityChunkSize; ++lx) {
                const int wx = baseX + lx;
                const int wy = baseY + ly;
                const int wz = baseZ + lz;

                if (grid.get(wx, wy, wz) >= kDensityThreshold) {
                    denseVoxels.push_back({wx, wy, wz});
                }
            }
        }
    }

    if (denseVoxels.empty()) {
        return true;
    }

    std::queue<std::array<int, 3>> queue;
    std::unordered_set<int64_t> supported;

    for (const auto& voxel : denseVoxels) {
        if (voxel[1] <= 0) {
            const int64_t key = packKey(voxel[0], voxel[1], voxel[2]);
            if (supported.insert(key).second) {
                queue.push(voxel);
            }
        }
    }

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();

        for (const auto& off : kNeighborOffsets) {
            const int nx = current[0] + off[0];
            const int ny = current[1] + off[1];
            const int nz = current[2] + off[2];

            if (grid.get(nx, ny, nz) < kDensityThreshold) {
                continue;
            }

            const int64_t nkey = packKey(nx, ny, nz);
            if (supported.insert(nkey).second) {
                queue.push({nx, ny, nz});
            }
        }
    }

    for (const auto& voxel : denseVoxels) {
        const int64_t key = packKey(voxel[0], voxel[1], voxel[2]);
        if (supported.count(key) == 0) {
            state.disconnectedVoxels.push_back(voxel);
        }
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
