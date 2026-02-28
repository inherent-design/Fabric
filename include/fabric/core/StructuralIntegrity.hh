#pragma once

#include <cstdint>
#include <functional>

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
    static int64_t packKey(int x, int y, int z) {
        return (static_cast<int64_t>(x) << 42) | (static_cast<int64_t>(y & 0x1FFFFF) << 21) |
               (static_cast<int64_t>(z & 0x1FFFFF));
    }

    void globalFloodFill(const ChunkedGrid<float>& grid);

    float perFrameBudgetMs_ = 1.0f;
    DebrisCallback debrisCallback_;
};

} // namespace fabric
