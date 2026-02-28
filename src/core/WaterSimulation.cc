#include "fabric/core/WaterSimulation.hh"

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/FieldLayer.hh"

#include <algorithm>
#include <cmath>

namespace fabric {

WaterSimulation::WaterSimulation()
    : current_(std::make_unique<FieldLayer<float>>()), next_(std::make_unique<FieldLayer<float>>()) {}

void WaterSimulation::setWaterChangeCallback(WaterChangeCallback cb) {
    changeCallback_ = std::move(cb);
}

void WaterSimulation::setPerFrameBudget(int maxCells) {
    perFrameBudget_ = maxCells;
}

int WaterSimulation::getPerFrameBudget() const {
    return perFrameBudget_;
}

int WaterSimulation::cellsProcessedLastStep() const {
    return cellsProcessed_;
}

FieldLayer<float>& WaterSimulation::waterField() {
    return *current_;
}

const FieldLayer<float>& WaterSimulation::waterField() const {
    return *current_;
}

void WaterSimulation::collectActiveCells(const ChunkedGrid<float>& density) {
    activeCells_.clear();
    activeCellsList_.clear();

    auto chunks = current_->grid().activeChunks();
    for (const auto& [cx, cy, cz] : chunks) {
        int baseX = cx * kChunkSize;
        int baseY = cy * kChunkSize;
        int baseZ = cz * kChunkSize;

        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int ly = 0; ly < kChunkSize; ++ly) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    int wx = baseX + lx;
                    int wy = baseY + ly;
                    int wz = baseZ + lz;
                    float level = current_->read(wx, wy, wz);
                    if (level > kMinWaterLevel) {
                        int64_t key = packKey(wx, wy, wz);
                        activeCells_.insert(key);
                    }
                }
            }
        }
    }

    // Also activate neighbors of water cells (they may receive water)
    std::vector<int64_t> neighborKeys;
    constexpr int offsets[5][3] = {{0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int64_t key : activeCells_) {
        int x, y, z;
        unpackKey(key, x, y, z);
        for (const auto& off : offsets) {
            int64_t nk = packKey(x + off[0], y + off[1], z + off[2]);
            neighborKeys.push_back(nk);
        }
    }
    for (int64_t nk : neighborKeys) {
        activeCells_.insert(nk);
    }

    activeCellsList_.reserve(activeCells_.size());
    for (int64_t key : activeCells_) {
        activeCellsList_.push_back(key);
    }
}

void WaterSimulation::step(const ChunkedGrid<float>& density, float dt) {
    (void)dt;

    collectActiveCells(density);

    // Copy current state into next buffer for cells that won't be processed
    for (int64_t key : activeCellsList_) {
        int x, y, z;
        unpackKey(key, x, y, z);
        next_->write(x, y, z, current_->read(x, y, z));
    }

    int limit = std::min(perFrameBudget_, static_cast<int>(activeCellsList_.size()));
    cellsProcessed_ = 0;

    for (int i = 0; i < limit; ++i) {
        int x, y, z;
        unpackKey(activeCellsList_[static_cast<size_t>(i)], x, y, z);
        applyWaterRules(x, y, z, density);
        ++cellsProcessed_;
    }

    // Emit change events and swap buffers
    for (int64_t key : activeCellsList_) {
        int x, y, z;
        unpackKey(key, x, y, z);
        float oldLevel = current_->read(x, y, z);
        float newLevel = next_->read(x, y, z);
        if (changeCallback_ && std::fabs(newLevel - oldLevel) > kMinWaterLevel) {
            changeCallback_(WaterChangeEvent{x, y, z, oldLevel, newLevel});
        }
    }

    std::swap(current_, next_);
}

void WaterSimulation::applyWaterRules(int x, int y, int z, const ChunkedGrid<float>& density) {
    if (density.get(x, y, z) >= kSolidThreshold)
        return;

    float myWater = current_->read(x, y, z);
    if (myWater <= kMinWaterLevel)
        return;

    float remaining = myWater;

    // Gravity: flow downward first
    bool belowSolid = density.get(x, y - 1, z) >= kSolidThreshold;
    if (!belowSolid) {
        float belowAccum = next_->read(x, y - 1, z);
        float space = 1.0f - belowAccum;
        float transfer = std::min(remaining * kGravityFlowRate, space);
        if (transfer > kMinWaterLevel) {
            next_->write(x, y - 1, z, std::min(belowAccum + transfer, 1.0f));
            remaining -= transfer;
        }
    }

    // Lateral spread to 4 horizontal neighbors with lower water level
    constexpr int lateral[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int eligible = 0;
    float myLevel = current_->read(x, y, z);
    for (const auto& off : lateral) {
        int nx = x + off[0];
        int nz = z + off[1];
        if (density.get(nx, y, nz) < kSolidThreshold && current_->read(nx, y, nz) < myLevel)
            ++eligible;
    }

    if (eligible > 0 && remaining > kMinWaterLevel) {
        float perNeighbor = (remaining * kFlowRate) / static_cast<float>(eligible);
        for (const auto& off : lateral) {
            int nx = x + off[0];
            int nz = z + off[1];
            if (density.get(nx, y, nz) >= kSolidThreshold || current_->read(nx, y, nz) >= myLevel)
                continue;
            float neighborAccum = next_->read(nx, y, nz);
            float space = 1.0f - neighborAccum;
            float transfer = std::min(perNeighbor, space);
            if (transfer > kMinWaterLevel) {
                next_->write(nx, y, nz, std::min(neighborAccum + transfer, 1.0f));
                remaining -= transfer;
            }
        }
    }

    // Write back remaining water, clamped and zeroed if negligible
    remaining = std::clamp(remaining, 0.0f, 1.0f);
    if (remaining < kMinWaterLevel)
        remaining = 0.0f;
    next_->write(x, y, z, remaining);
}

} // namespace fabric
