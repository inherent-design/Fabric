#include "fabric/core/StructuralIntegrity.hh"

#include "fabric/core/ChunkedGrid.hh"

#include <chrono>
#include <queue>
#include <tuple>
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

    // Global BFS across all active chunks at once
    FloodFillState state;
    floodFillGlobal(grid, active, state);
    processFloodFillResults(grid, state);
    checkedChunks_.clear();
}

void StructuralIntegrity::floodFillGlobal(const ChunkedGrid<float>& grid,
                                          const std::vector<std::tuple<int, int, int>>& chunks, FloodFillState& state) {
    constexpr float kDensityThreshold = 0.5f;
    constexpr int kNeighborOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    // Collect all dense voxels across every active chunk
    std::unordered_set<int64_t> denseSet;
    std::vector<std::array<int, 3>> denseVoxels;

    for (const auto& [cx, cy, cz] : chunks) {
        const int baseX = cx * kStructuralIntegrityChunkSize;
        const int baseY = cy * kStructuralIntegrityChunkSize;
        const int baseZ = cz * kStructuralIntegrityChunkSize;

        for (int lz = 0; lz < kStructuralIntegrityChunkSize; ++lz) {
            for (int ly = 0; ly < kStructuralIntegrityChunkSize; ++ly) {
                for (int lx = 0; lx < kStructuralIntegrityChunkSize; ++lx) {
                    const int wx = baseX + lx;
                    const int wy = baseY + ly;
                    const int wz = baseZ + lz;

                    if (grid.get(wx, wy, wz) >= kDensityThreshold) {
                        denseVoxels.push_back({wx, wy, wz});
                        denseSet.insert(packKey(wx, wy, wz));
                    }
                }
            }
        }
    }

    if (denseVoxels.empty()) {
        return;
    }

    // Seed BFS from all ground-level voxels (y <= 0) across all chunks
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

    // BFS walks freely across chunk boundaries via grid.get()
    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();

        for (const auto& off : kNeighborOffsets) {
            const int nx = current[0] + off[0];
            const int ny = current[1] + off[1];
            const int nz = current[2] + off[2];

            const int64_t nkey = packKey(nx, ny, nz);

            // Only expand into voxels that belong to active chunks
            if (denseSet.count(nkey) == 0) {
                continue;
            }

            if (supported.insert(nkey).second) {
                queue.push({nx, ny, nz});
            }
        }
    }

    // Any dense voxel not reached by BFS is disconnected
    for (const auto& voxel : denseVoxels) {
        const int64_t key = packKey(voxel[0], voxel[1], voxel[2]);
        if (supported.count(key) == 0) {
            state.disconnectedVoxels.push_back(voxel);
        }
    }
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
