#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <queue>
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

struct FloodFillState {
    std::queue<std::array<int, 3>> queue;
    std::unordered_set<int64_t> supported;
    std::vector<std::array<int, 3>> allDenseVoxels;
    uint64_t processedCells = 0;
    bool inProgress = false;

    void reset() {
        queue = {};
        supported.clear();
        allDenseVoxels.clear();
        processedCells = 0;
        inProgress = false;
    }
};

class StructuralIntegrity {
  public:
    StructuralIntegrity();
    ~StructuralIntegrity() = default;

    void update(const ChunkedGrid<float>& grid, float dt);
    void setDebrisCallback(DebrisCallback cb);
    void setPerFrameBudgetMs(float budgetMs);
    float getPerFrameBudgetMs() const;
    uint64_t getProcessedCells() const;

  private:
    static int64_t packKey(int x, int y, int z) {
        return (static_cast<int64_t>(x) << 42) | (static_cast<int64_t>(y & 0x1FFFFF) << 21) |
               (static_cast<int64_t>(z & 0x1FFFFF));
    }

    bool globalFloodFill(const ChunkedGrid<float>& grid);

    float perFrameBudgetMs_ = 1.0f;
    DebrisCallback debrisCallback_;
    FloodFillState floodFillState_;
};

} // namespace fabric
