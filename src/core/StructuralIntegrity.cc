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

uint64_t StructuralIntegrity::getProcessedCells() const {
    return floodFillState_.processedCells;
}

void StructuralIntegrity::update(const ChunkedGrid<float>& grid, float dt) {
    (void)dt;

    if (perFrameBudgetMs_ <= 0.0f || !debrisCallback_) {
        return;
    }

    if (!floodFillState_.inProgress) {
        const auto active = grid.activeChunks();
        if (active.empty()) {
            return;
        }
    }

    globalFloodFill(grid);
}

bool StructuralIntegrity::globalFloodFill(const ChunkedGrid<float>& grid) {
    constexpr float kDensityThreshold = 0.5f;
    constexpr int kNeighborOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    constexpr int kBudgetCheckInterval = 256;

    const auto budgetNs = static_cast<int64_t>(perFrameBudgetMs_ * 1'000'000.0f);
    const auto startTime = std::chrono::high_resolution_clock::now();

    // Phase 1: Collect dense voxels and seed ground (only on fresh pass)
    if (!floodFillState_.inProgress) {
        floodFillState_.reset();

        const auto active = grid.activeChunks();
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
                            floodFillState_.allDenseVoxels.push_back({wx, wy, wz});
                            if (wy <= 0) {
                                const int64_t key = packKey(wx, wy, wz);
                                if (floodFillState_.supported.insert(key).second) {
                                    floodFillState_.queue.push({wx, wy, wz});
                                }
                            }
                        }
                    }
                }
            }
        }

        if (floodFillState_.allDenseVoxels.empty()) {
            return true;
        }

        floodFillState_.inProgress = true;
    }

    // Phase 2: Budget-limited BFS
    int popsSinceCheck = 0;
    while (!floodFillState_.queue.empty()) {
        const auto current = floodFillState_.queue.front();
        floodFillState_.queue.pop();
        ++floodFillState_.processedCells;
        ++popsSinceCheck;

        for (const auto& off : kNeighborOffsets) {
            const int nx = current[0] + off[0];
            const int ny = current[1] + off[1];
            const int nz = current[2] + off[2];

            if (grid.get(nx, ny, nz) < kDensityThreshold) {
                continue;
            }

            const int64_t nkey = packKey(nx, ny, nz);
            if (floodFillState_.supported.insert(nkey).second) {
                floodFillState_.queue.push({nx, ny, nz});
            }
        }

        if (popsSinceCheck >= kBudgetCheckInterval) {
            popsSinceCheck = 0;
            const auto now = std::chrono::high_resolution_clock::now();
            const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime).count();
            if (elapsedNs >= budgetNs) {
                return false;
            }
        }
    }

    // Phase 3: BFS complete -- report disconnected voxels as debris
    for (const auto& voxel : floodFillState_.allDenseVoxels) {
        const int64_t key = packKey(voxel[0], voxel[1], voxel[2]);
        if (floodFillState_.supported.count(key) == 0) {
            debrisCallback_(DebrisEvent{voxel[0], voxel[1], voxel[2], grid.get(voxel[0], voxel[1], voxel[2])});
        }
    }

    floodFillState_.inProgress = false;
    return true;
}

} // namespace fabric
