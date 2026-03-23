#include "recurse/simulation/TransformationPass.hh"
#include "fabric/log/Log.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include <algorithm>
#include <cmath>

namespace recurse::simulation {

TransformationPass::TransformationPass(const WorldRuleEngine& rules, const MaterialRegistry& registry,
                                       SimulationGrid& grid, const GhostCellManager& ghosts,
                                       ChunkActivityTracker& tracker)
    : rules_(rules), registry_(registry), grid_(grid), ghosts_(ghosts), tracker_(tracker) {
    FABRIC_LOG_DEBUG("TransformationPass initialized: diffusionRate={}, maxTransforms={}", config_.diffusionRate,
                     config_.maxTransformsPerChunk);
}

void TransformationPass::execute(const std::vector<ActiveChunkEntry>& active, fabric::JobScheduler& scheduler,
                                 int64_t worldSeed) {
    FABRIC_ZONE_SCOPED_N("phase_3c_transform");
    totalTransforms_.store(0, std::memory_order_relaxed);

    scheduler.parallelFor(active.size(), "phase_3c_transform", [&](size_t jobIdx, size_t /*workerIdx*/) {
        const auto& pos = active[jobIdx].pos;
        std::mt19937 rng(static_cast<uint32_t>(worldSeed ^ spatialHash(pos)));
        executeChunk(pos, rng);
    });

    int transforms = totalTransforms_.load(std::memory_order_relaxed);
    if (transforms > 0) {
        FABRIC_LOG_DEBUG("Phase 3c: {} chunks, {} transforms", active.size(), transforms);
    }
}

void TransformationPass::executeChunk(ChunkCoord pos, std::mt19937& rng) {
    thermalKernel(pos);
    int count = ruleEvaluation(pos, rng);
    if (count > 0) {
        totalTransforms_.fetch_add(count, std::memory_order_relaxed);
    }
}

VoxelCell TransformationPass::readCell(ChunkCoord pos, int lx, int ly, int lz) const {
    if (lx >= 0 && lx < K_CHUNK_SIZE && ly >= 0 && ly < K_CHUNK_SIZE && lz >= 0 && lz < K_CHUNK_SIZE) {
        int wx = pos.x * K_CHUNK_SIZE + lx;
        int wy = pos.y * K_CHUNK_SIZE + ly;
        int wz = pos.z * K_CHUNK_SIZE + lz;
        return grid_.readFromWriteBuffer(wx, wy, wz);
    }
    return ghosts_.readGhost(pos, lx, ly, lz);
}

void TransformationPass::thermalKernel(ChunkCoord pos) {
    FABRIC_ZONE_SCOPED_N("thermalKernel");

    static constexpr int offsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                int wx = pos.x * K_CHUNK_SIZE + lx;
                int wy = pos.y * K_CHUNK_SIZE + ly;
                int wz = pos.z * K_CHUNK_SIZE + lz;

                VoxelCell cell = grid_.readFromWriteBuffer(wx, wy, wz);
                if (isEmpty(cell))
                    continue;

                uint8_t selfTemp = cellTemperature(cell);
                uint8_t selfCond = registry_.get(static_cast<MaterialId>(cell.essenceIdx)).thermalConductivity;

                // Read 6 face-neighbor temperatures and conductivities
                float neighborTempSum = 0.0f;
                float neighborCondSum = 0.0f;
                int neighborCount = 0;
                bool allEqual = true;

                for (const auto& off : offsets) {
                    VoxelCell neighbor = readCell(pos, lx + off[0], ly + off[1], lz + off[2]);
                    if (isEmpty(neighbor))
                        continue; // Air gaps are thermal insulators

                    uint8_t nTemp = cellTemperature(neighbor);
                    if (nTemp != selfTemp)
                        allEqual = false;
                    neighborTempSum += static_cast<float>(nTemp);
                    neighborCondSum += static_cast<float>(
                        registry_.get(static_cast<MaterialId>(neighbor.essenceIdx)).thermalConductivity);
                    ++neighborCount;
                }

                // Skip if isolated in air or thermally stable
                if (neighborCount == 0 || allEqual)
                    continue;

                float neighborAvgTemp = neighborTempSum / static_cast<float>(neighborCount);
                float neighborAvgCond = neighborCondSum / static_cast<float>(neighborCount);

                // Min-of-pair conductivity model, normalized to 0.0-1.0
                float effCond = std::min(static_cast<float>(selfCond), neighborAvgCond) / 255.0f;

                float newTempF = static_cast<float>(selfTemp) +
                                 (neighborAvgTemp - static_cast<float>(selfTemp)) * effCond * config_.diffusionRate;

                // Clamp to [0, 255]
                uint8_t newTemp = static_cast<uint8_t>(std::clamp(newTempF, 0.0f, 255.0f));

                if (newTemp != selfTemp) {
                    setCellTemperature(cell, newTemp);
                    grid_.writeCell(wx, wy, wz, cell);
                }
            }
        }
    }
}

int TransformationPass::ruleEvaluation(ChunkCoord pos, std::mt19937& rng) {
    FABRIC_ZONE_SCOPED_N("ruleEvaluation");

    int transformCount = 0;
    std::vector<WorldRule> matchBuffer;

    for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                if (transformCount >= config_.maxTransformsPerChunk)
                    return transformCount;

                int wx = pos.x * K_CHUNK_SIZE + lx;
                int wy = pos.y * K_CHUNK_SIZE + ly;
                int wz = pos.z * K_CHUNK_SIZE + lz;

                VoxelCell cell = grid_.readFromWriteBuffer(wx, wy, wz);
                if (isEmpty(cell))
                    continue;

                uint8_t temp = cellTemperature(cell);
                bool transformed = false;

                // Self-transform rules (neighborEssence = 255)
                rules_.query(cell.essenceIdx, 255, cell.phase(), temp, matchBuffer);
                for (const auto& rule : matchBuffer) {
                    if (transformed)
                        break;
                    uint8_t roll = static_cast<uint8_t>(rng() & 0xFF);
                    if (roll < rule.probability) {
                        if (rule.resultEssenceA != 255)
                            cell.essenceIdx = rule.resultEssenceA;
                        if (rule.resultPhaseA != Phase::Unchanged)
                            cell.setPhase(rule.resultPhaseA);
                        if (rule.resultTempA != 0)
                            setCellTemperature(cell, rule.resultTempA);
                        // Update displacement rank from registry for new essence
                        if (rule.resultEssenceA != 255) {
                            cell.displacementRank = registry_.get(static_cast<MaterialId>(rule.resultEssenceA)).density;
                        }
                        grid_.writeCell(wx, wy, wz, cell);
                        if (cell.phase() == Phase::Liquid || cell.phase() == Phase::Powder)
                            tracker_.markSubRegionActive(pos, lx, ly, lz);
                        ++transformCount;
                        transformed = true;
                    }
                }

                if (transformed)
                    continue;

                // Contact rules: check 6 face-neighbors
                static constexpr int offsets[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                                      {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
                for (const auto& off : offsets) {
                    if (transformed)
                        break;

                    int nlx = lx + off[0];
                    int nly = ly + off[1];
                    int nlz = lz + off[2];

                    VoxelCell neighbor = readCell(pos, nlx, nly, nlz);
                    if (isEmpty(neighbor))
                        continue;

                    rules_.query(cell.essenceIdx, neighbor.essenceIdx, cell.phase(), temp, matchBuffer);
                    for (const auto& rule : matchBuffer) {
                        if (transformed)
                            break;
                        uint8_t roll = static_cast<uint8_t>(rng() & 0xFF);
                        if (roll < rule.probability) {
                            // Apply to self
                            if (rule.resultEssenceA != 255)
                                cell.essenceIdx = rule.resultEssenceA;
                            if (rule.resultPhaseA != Phase::Unchanged)
                                cell.setPhase(rule.resultPhaseA);
                            if (rule.resultTempA != 0)
                                setCellTemperature(cell, rule.resultTempA);
                            if (rule.resultEssenceA != 255) {
                                cell.displacementRank =
                                    registry_.get(static_cast<MaterialId>(rule.resultEssenceA)).density;
                            }
                            grid_.writeCell(wx, wy, wz, cell);
                            if (cell.phase() == Phase::Liquid || cell.phase() == Phase::Powder)
                                tracker_.markSubRegionActive(pos, lx, ly, lz);

                            // Apply to neighbor (skip cross-chunk writes)
                            bool neighborInBounds = nlx >= 0 && nlx < K_CHUNK_SIZE && nly >= 0 && nly < K_CHUNK_SIZE &&
                                                    nlz >= 0 && nlz < K_CHUNK_SIZE;
                            if (neighborInBounds && (rule.resultEssenceB != 255 ||
                                                     rule.resultPhaseB != Phase::Unchanged || rule.resultTempB != 0)) {
                                int nwx = pos.x * K_CHUNK_SIZE + nlx;
                                int nwy = pos.y * K_CHUNK_SIZE + nly;
                                int nwz = pos.z * K_CHUNK_SIZE + nlz;
                                VoxelCell nCell = grid_.readFromWriteBuffer(nwx, nwy, nwz);
                                if (rule.resultEssenceB != 255)
                                    nCell.essenceIdx = rule.resultEssenceB;
                                if (rule.resultPhaseB != Phase::Unchanged)
                                    nCell.setPhase(rule.resultPhaseB);
                                if (rule.resultTempB != 0)
                                    setCellTemperature(nCell, rule.resultTempB);
                                if (rule.resultEssenceB != 255) {
                                    nCell.displacementRank =
                                        registry_.get(static_cast<MaterialId>(rule.resultEssenceB)).density;
                                }
                                grid_.writeCell(nwx, nwy, nwz, nCell);
                                if (nCell.phase() == Phase::Liquid || nCell.phase() == Phase::Powder)
                                    tracker_.markSubRegionActive(pos, nlx, nly, nlz);
                            }

                            ++transformCount;
                            transformed = true;
                        }
                    }
                }
            }
        }
    }
    return transformCount;
}

} // namespace recurse::simulation
