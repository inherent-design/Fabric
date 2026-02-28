#pragma once

#include "fabric/core/ChunkedGrid.hh"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <unordered_set>

namespace fabric {

/// Result of BFS flood-fill zone estimation.
struct ZoneEstimate {
    int volume = 0;        ///< Number of air voxels visited.
    int surfaceArea = 0;   ///< Number of solid-neighbor faces counted.
    float openness = 0.0f; ///< Fraction of frontier voxels at budget limit (0=sealed, 1=open).
    bool complete = true;  ///< True if BFS exhausted all reachable air within budget.
};

/// Reverb parameters derived from zone estimation.
struct ReverbParams {
    float decayTime = 0.0f; ///< RT60 in seconds, clamped to [0.1, 3.0].
    float damping = 0.0f;   ///< Absorption factor [0.1, 0.9].
    float wetMix = 0.0f;    ///< Wet/dry ratio [0.0, 1.0].
};

/// One-shot BFS flood-fill zone estimation.
/// Walks 6-connected air voxels from start position, counting volume,
/// surface area, and openness up to a budget cap.
ZoneEstimate estimateZone(const ChunkedGrid<float>& density, int startX, int startY, int startZ, float threshold,
                          int maxVoxels);

/// Map a zone estimate to reverb parameters using the Sabine equation.
ReverbParams mapToReverbParams(const ZoneEstimate& zone, float voxelSize = 1.0f);

/// Incremental BFS zone estimator that can spread work across frames.
class ReverbZoneEstimator {
  public:
    ReverbZoneEstimator() = default;

    /// Reset BFS state and start from a new position.
    void reset(int startX, int startY, int startZ);

    /// Process up to `budget` voxels of BFS.
    void advanceBFS(const ChunkedGrid<float>& density, float threshold, int budget);

    /// Current zone estimate (may be partial if BFS is incomplete).
    ZoneEstimate estimate() const;

    /// True when BFS has no more frontier to explore.
    bool isComplete() const;

  private:
    /// Pack 3 ints into a single 64-bit key for the visited set.
    static int64_t packCoord(int x, int y, int z);

    std::deque<std::array<int, 3>> queue_;
    std::unordered_set<int64_t> visited_;

    int volume_ = 0;
    int surfaceArea_ = 0;
    int frontierCount_ = 0; ///< Voxels remaining in queue when budget exhausted.
    bool started_ = false;
    bool complete_ = true;
};

} // namespace fabric
