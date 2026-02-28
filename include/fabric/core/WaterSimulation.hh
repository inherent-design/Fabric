#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

namespace fabric {

template <typename T> class ChunkedGrid;
template <typename T> class FieldLayer;

struct WaterChangeEvent {
    int x, y, z;
    float oldLevel;
    float newLevel;
};

using WaterChangeCallback = std::function<void(const WaterChangeEvent&)>;

class WaterSimulation {
  public:
    WaterSimulation();
    ~WaterSimulation() = default;

    void step(const ChunkedGrid<float>& density, float dt);

    FieldLayer<float>& waterField();
    const FieldLayer<float>& waterField() const;

    void setWaterChangeCallback(WaterChangeCallback cb);
    void setPerFrameBudget(int maxCells);
    int getPerFrameBudget() const;

    int cellsProcessedLastStep() const;

  private:
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

    void collectActiveCells(const ChunkedGrid<float>& density);
    void applyWaterRules(int x, int y, int z, const ChunkedGrid<float>& density);

    // Double-buffer: read from current_, write to next_, swap at end of step
    std::unique_ptr<FieldLayer<float>> current_;
    std::unique_ptr<FieldLayer<float>> next_;

    // Active cells: cells with water or adjacent to water
    std::unordered_set<int64_t> activeCells_;
    std::vector<int64_t> activeCellsList_;

    WaterChangeCallback changeCallback_;
    int perFrameBudget_ = 4096;
    int cellsProcessed_ = 0;

    static constexpr float kMinWaterLevel = 0.001f;
    static constexpr float kFlowRate = 0.25f;
    static constexpr float kGravityFlowRate = 0.5f;
    static constexpr float kSolidThreshold = 0.5f;
};

} // namespace fabric
