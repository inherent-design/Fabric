#include "recurse/simulation/FunctionExecutor.hh"
#include "fabric/log/Log.hh"
#include "recurse/simulation/CellAccessors.hh"

namespace recurse::simulation {

FunctionExecutor::FunctionExecutor(SimulationGrid& grid, int64_t maxCells)
    : grid_(grid), maxCells_(maxCells), budgetRemaining_(maxCells) {}

FunctionExecutorResult FunctionExecutor::execute(FunctionOp op, int wx, int wy, int wz, VoxelCell newCell,
                                                 int64_t budget) {
    FunctionExecutorResult result;
    result.budgetRemaining = budgetRemaining_;

    int64_t effectiveBudget = budget >= 0 ? budget : budgetRemaining_;
    if (effectiveBudget <= 0) {
        result.budgetExceeded = true;
        return result;
    }

    VoxelCell existing = grid_.readCell(wx, wy, wz);

    switch (op) {
        case FunctionOp::Create:
            if (!isEmpty(existing)) {
                return result;
            }
            grid_.writeCell(wx, wy, wz, newCell);
            break;

        case FunctionOp::Destroy:
            if (isEmpty(existing)) {
                return result;
            }
            grid_.writeCell(wx, wy, wz, VoxelCell{});
            break;

        case FunctionOp::Transform:
            if (isEmpty(existing)) {
                return result;
            }
            grid_.writeCell(wx, wy, wz, newCell);
            break;
    }

    budgetRemaining_ -= 1;
    totalModified_ += 1;
    result.cellsModified = 1;
    result.budgetRemaining = budgetRemaining_;
    return result;
}

void FunctionExecutor::resetBudget() {
    budgetRemaining_ = maxCells_;
}

void FunctionExecutor::setMaxCells(int64_t max) {
    maxCells_ = max;
    if (budgetRemaining_ > maxCells_)
        budgetRemaining_ = maxCells_;
}

} // namespace recurse::simulation
