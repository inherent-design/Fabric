#pragma once
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/world/FunctionContracts.hh"
#include <cstdint>

namespace recurse::simulation {

enum class FunctionOp : uint8_t {
    Create,
    Destroy,
    Transform,
};

struct FunctionExecutorResult {
    int64_t cellsModified = 0;
    int64_t budgetRemaining = 0;
    bool budgetExceeded = false;
};

/// Thin executor wrapping SimulationGrid::writeCell with budget enforcement.
/// Routes through the same write path as WorldRuleEngine rules for consistency.
class FunctionExecutor {
  public:
    static constexpr int64_t kDefaultMaxCells = 256;

    explicit FunctionExecutor(SimulationGrid& grid, int64_t maxCells = kDefaultMaxCells);

    FunctionExecutorResult execute(FunctionOp op, int wx, int wy, int wz, VoxelCell newCell, int64_t budget = -1);

    void resetBudget();

    int64_t totalModified() const { return totalModified_; }
    int64_t budgetRemaining() const { return budgetRemaining_; }
    int64_t maxCells() const { return maxCells_; }
    void setMaxCells(int64_t max);

  private:
    SimulationGrid& grid_;
    int64_t maxCells_;
    int64_t budgetRemaining_;
    int64_t totalModified_ = 0;
};

} // namespace recurse::simulation
