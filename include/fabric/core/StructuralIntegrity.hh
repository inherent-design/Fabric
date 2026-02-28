#pragma once

#include <array>
#include <cstdint>
#include <functional>
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

  private:
    struct FloodFillState {
        int64_t processedCells = 0;
        std::unordered_set<int64_t> visited;
        std::vector<std::array<int, 3>> disconnectedVoxels;
    };

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
                        float timeBudgetNs);

    void processFloodFillResults(const ChunkedGrid<float>& grid, FloodFillState& state);

    float perFrameBudgetMs_ = 1.0f;
    DebrisCallback debrisCallback_;
    std::unordered_map<int64_t, uint8_t> checkedChunks_;
};

} // namespace fabric
