#include "recurse/world/StructuralIntegrity.hh"

#include "fabric/world/ChunkedGrid.hh"

#include <chrono>

namespace recurse {

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

void StructuralIntegrity::update(const ChunkedGrid<float, 32>& grid, float dt) {
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

bool StructuralIntegrity::globalFloodFill(const ChunkedGrid<float, 32>& grid) {
    constexpr float K_DENSITY_THRESHOLD = 0.5f;
    constexpr int K_BUDGET_CHECK_INTERVAL = 256;

    const auto budgetNs = static_cast<int64_t>(perFrameBudgetMs_ * 1'000'000.0f);
    const auto startTime = std::chrono::high_resolution_clock::now();

    // Phase 1: Collect dense voxels and seed ground (only on fresh pass)
    if (!floodFillState_.inProgress) {
        floodFillState_.reset();

        const auto active = grid.activeChunks();
        for (const auto& [cx, cy, cz] : active) {
            const int baseX = cx * K_CHUNK_SIZE;
            const int baseY = cy * K_CHUNK_SIZE;
            const int baseZ = cz * K_CHUNK_SIZE;

            for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
                for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
                    for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                        const int wx = baseX + lx;
                        const int wy = baseY + ly;
                        const int wz = baseZ + lz;

                        if (grid.get(wx, wy, wz) >= K_DENSITY_THRESHOLD) {
                            floodFillState_.allDenseVoxels.push_back({wx, wy, wz});
                            if (wy <= 0) {
                                const int64_t key = static_cast<int64_t>(packChunkKey(wx, wy, wz));
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

        for (const auto& off : K_FACE_NEIGHBORS) {
            const int nx = current[0] + off[0];
            const int ny = current[1] + off[1];
            const int nz = current[2] + off[2];

            if (grid.get(nx, ny, nz) < K_DENSITY_THRESHOLD) {
                continue;
            }

            const int64_t nkey = static_cast<int64_t>(packChunkKey(nx, ny, nz));
            if (floodFillState_.supported.insert(nkey).second) {
                floodFillState_.queue.push({nx, ny, nz});
            }
        }

        if (popsSinceCheck >= K_BUDGET_CHECK_INTERVAL) {
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
        const int64_t key = static_cast<int64_t>(packChunkKey(voxel[0], voxel[1], voxel[2]));
        if (floodFillState_.supported.count(key) == 0) {
            const float density = grid.get(voxel[0], voxel[1], voxel[2]);
            if (density > 0.0f) {
                debrisCallback_(DebrisEvent{voxel[0], voxel[1], voxel[2], density});
            }
        }
    }

    floodFillState_.inProgress = false;
    return true;
}

} // namespace recurse
