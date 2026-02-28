#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fabric {

template <typename T> class ChunkedGrid;

inline constexpr int kStructuralIntegrityChunkSize = 32;

struct DebrisEvent {
    int x, y, z;
    float density;
};

using DebrisCallback = std::function<void(const DebrisEvent&)>;

class StructuralIntegrity {
  public:
    StructuralIntegrity();
    ~StructuralIntegrity() = default;

    void update(const ChunkedGrid<float>& grid, float dt);
    void setDebrisCallback(DebrisCallback cb);
    void setPerFrameBudgetMs(float budgetMs);
    float getPerFrameBudgetMs() const;

    struct FloodFillState {
        enum class Phase : uint8_t {
            EnumerateVoxels,
            SeedGround,
            BFS,
            CollectUnsupported,
            Done
        };

        Phase phase = Phase::EnumerateVoxels;
        int64_t processedCells = 0;
        std::vector<std::array<int, 3>> denseVoxels;
        std::queue<std::array<int, 3>> queue;
        std::unordered_set<int64_t> supported;
        std::vector<std::array<int, 3>> disconnectedVoxels;
    };

    // Exposed for testing: returns partial state for a chunk, or nullptr if none
    const FloodFillState* getPartialState(int64_t chunkKey) const;

    static int64_t packKey(int x, int y, int z) {
        return (static_cast<int64_t>(x) << 42) | (static_cast<int64_t>(y & 0x1FFFFF) << 21) |
               (static_cast<int64_t>(z & 0x1FFFFF));
    }

    static void unpackKey(int64_t key, int& x, int& y, int& z) {
        x = static_cast<int>(key >> 42);
        y = static_cast<int>((key >> 21) & 0x1FFFFF);
        z = static_cast<int>(key & 0x1FFFFF);
        if (y & 0x100000)
            y |= ~0x1FFFFF;
        if (z & 0x100000)
            z |= ~0x1FFFFF;
    }

    bool floodFillChunk(int cx, int cy, int cz, const ChunkedGrid<float>& grid, FloodFillState& state,
                        int64_t timeBudgetNs);

  private:
    static constexpr int kTimingCheckInterval = 256;

    void processFloodFillResults(const ChunkedGrid<float>& grid, FloodFillState& state);

    float perFrameBudgetMs_ = 1.0f;
    DebrisCallback debrisCallback_;
    std::unordered_map<int64_t, uint8_t> checkedChunks_;
    std::unordered_map<int64_t, FloodFillState> partialStates_;
};

} // namespace fabric
