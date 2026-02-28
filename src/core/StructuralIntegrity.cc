#include "fabric/core/StructuralIntegrity.hh"

#include "fabric/core/ChunkedGrid.hh"

#include <array>
#include <queue>
#include <unordered_set>
#include <vector>

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
        return;
    }

    globalFloodFill(grid);
}

void StructuralIntegrity::globalFloodFill(const ChunkedGrid<float>& grid) {
    constexpr float kDensityThreshold = 0.5f;
    constexpr int kNeighborOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    const auto active = grid.activeChunks();

    // Phase 1: Collect all dense voxels across all chunks, seed BFS from ground
    std::vector<std::array<int, 3>> allDenseVoxels;
    std::queue<std::array<int, 3>> queue;
    std::unordered_set<int64_t> supported;

    for (const auto& [cx, cy, cz] : active) {
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
                        allDenseVoxels.push_back({wx, wy, wz});
                        if (wy <= 0) {
                            const int64_t key = packKey(wx, wy, wz);
                            if (supported.insert(key).second) {
                                queue.push({wx, wy, wz});
                            }
                        }
                    }
                }
            }
        }
    }

    if (allDenseVoxels.empty()) {
        return;
    }

    // Phase 2: Global BFS -- crosses chunk boundaries via grid.get()
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

    // Phase 3: Any dense voxel not reached by BFS is disconnected
    for (const auto& voxel : allDenseVoxels) {
        const int64_t key = packKey(voxel[0], voxel[1], voxel[2]);
        if (supported.count(key) == 0) {
            debrisCallback_(DebrisEvent{voxel[0], voxel[1], voxel[2], grid.get(voxel[0], voxel[1], voxel[2])});
        }
    }
}

} // namespace fabric
