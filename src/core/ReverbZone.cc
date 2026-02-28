#include "fabric/core/ReverbZone.hh"

#include <algorithm>
#include <array>
#include <cmath>

namespace fabric {

// ---------- Coordinate packing ----------

int64_t ReverbZoneEstimator::packCoord(int x, int y, int z) {
    // Same packing scheme as ChunkedGrid::packKey but at voxel level.
    return (static_cast<int64_t>(x) << 42) | (static_cast<int64_t>(y & 0x1FFFFF) << 21) |
           static_cast<int64_t>(z & 0x1FFFFF);
}

// ---------- 6-connected neighbor offsets ----------

static constexpr std::array<std::array<int, 3>, 6> kNeighborOffsets = {{
    {{1, 0, 0}},
    {{-1, 0, 0}},
    {{0, 1, 0}},
    {{0, -1, 0}},
    {{0, 0, 1}},
    {{0, 0, -1}},
}};

// ---------- One-shot estimateZone ----------

ZoneEstimate estimateZone(const ChunkedGrid<float>& density, int startX, int startY, int startZ, float threshold,
                          int maxVoxels) {
    ReverbZoneEstimator estimator;
    estimator.reset(startX, startY, startZ);
    estimator.advanceBFS(density, threshold, maxVoxels);
    return estimator.estimate();
}

// ---------- ReverbZoneEstimator ----------

void ReverbZoneEstimator::reset(int startX, int startY, int startZ) {
    queue_.clear();
    visited_.clear();
    volume_ = 0;
    surfaceArea_ = 0;
    frontierCount_ = 0;
    started_ = true;
    complete_ = false;

    queue_.push_back({startX, startY, startZ});
    visited_.insert(packCoord(startX, startY, startZ));
}

void ReverbZoneEstimator::advanceBFS(const ChunkedGrid<float>& density, float threshold, int budget) {
    if (!started_ || complete_)
        return;

    int processed = 0;

    while (!queue_.empty() && processed < budget) {
        auto [x, y, z] = queue_.front();
        queue_.pop_front();

        float d = density.get(x, y, z);
        if (d >= threshold) {
            // This voxel is solid; skip it.
            continue;
        }

        // Air voxel: count it.
        ++volume_;
        ++processed;

        // Check 6-connected neighbors.
        for (const auto& off : kNeighborOffsets) {
            int nx = x + off[0];
            int ny = y + off[1];
            int nz = z + off[2];

            float nd = density.get(nx, ny, nz);
            if (nd >= threshold) {
                // Solid neighbor: contributes to surface area.
                ++surfaceArea_;
            } else {
                // Air neighbor: enqueue if not visited.
                int64_t key = packCoord(nx, ny, nz);
                if (visited_.insert(key).second) {
                    queue_.push_back({nx, ny, nz});
                }
            }
        }
    }

    if (queue_.empty()) {
        complete_ = true;
        frontierCount_ = 0;
    } else {
        complete_ = false;
        frontierCount_ = static_cast<int>(queue_.size());
    }
}

ZoneEstimate ReverbZoneEstimator::estimate() const {
    ZoneEstimate est;
    est.volume = volume_;
    est.surfaceArea = surfaceArea_;
    est.complete = complete_;

    if (complete_ || volume_ == 0) {
        // BFS completed or never started: no frontier remains.
        // A fully sealed region finishes BFS with complete=true.
        est.openness = 0.0f;
    } else {
        // BFS incomplete (budget-capped). Openness measures how unbounded the
        // region is: 1 - (solid_faces / total_face_checks). In a fully open
        // area surfaceArea=0 so openness=1.0. In a partially explored room
        // with walls nearby, surfaceArea grows relative to volume.
        float totalFaceChecks = 6.0f * static_cast<float>(std::max(volume_, 1));
        est.openness = std::clamp(1.0f - static_cast<float>(surfaceArea_) / totalFaceChecks, 0.0f, 1.0f);
    }

    return est;
}

bool ReverbZoneEstimator::isComplete() const {
    return !started_ || complete_;
}

// ---------- mapToReverbParams ----------

ReverbParams mapToReverbParams(const ZoneEstimate& zone, float voxelSize) {
    ReverbParams params;

    if (zone.volume <= 0) {
        params.decayTime = 0.1f;
        params.damping = 0.9f;
        params.wetMix = 0.0f;
        return params;
    }

    float vs3 = voxelSize * voxelSize * voxelSize;
    float vs2 = voxelSize * voxelSize;
    float V = static_cast<float>(zone.volume) * vs3;
    float S = static_cast<float>(zone.surfaceArea) * vs2;

    // Sabine equation: RT60 = 0.161 * V / (alpha * S)
    constexpr float kAlphaAvg = 0.3f; // moderate absorption (stone/concrete)
    float rt60 = 0.0f;
    if (S > 0.0f) {
        rt60 = 0.161f * V / (kAlphaAvg * S);
    }
    rt60 = std::clamp(rt60, 0.1f, 3.0f);

    // Damping: ratio of surface area to volume (more surface = more absorption).
    float damping =
        std::clamp(static_cast<float>(zone.surfaceArea) / static_cast<float>(std::max(zone.volume, 1)), 0.1f, 0.9f);

    // Wet mix: proportional to decay time, reduced by openness (sound escapes).
    float wetMix = std::clamp(rt60 / 3.0f, 0.0f, 1.0f) * (1.0f - 0.5f * zone.openness);

    params.decayTime = rt60;
    params.damping = damping;
    params.wetMix = wetMix;
    return params;
}

} // namespace fabric
