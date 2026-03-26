#pragma once
#include "fabric/platform/JobScheduler.hh"
#include "recurse/simulation/BoundaryWriteQueue.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/WorldRuleEngine.hh"
#include <atomic>
#include <cstdint>
#include <random>
#include <vector>

namespace recurse::simulation {

struct ActiveChunkEntry;
struct CellSwap;

struct SubRegionActivation {
    ChunkCoord pos;
    int lx, ly, lz;
};

struct EvaluateChunkResult {
    bool anyGravityMovement = false;
    int transformCount = 0;
};

class TransformationPass {
  public:
    struct Config {
        float diffusionRate = 0.25f;
        int maxTransformsPerChunk = K_MAX_TRANSFORMS_PER_CHUNK;
    };

    TransformationPass(const WorldRuleEngine& rules, const MaterialRegistry& registry, SimulationGrid& grid,
                       const GhostCellManager& ghosts, ChunkActivityTracker& tracker);

    /// Unified dispatch: thermal pre-pass then all rules per chunk (parallel across chunks).
    void execute(const std::vector<ActiveChunkEntry>& active, fabric::JobScheduler& scheduler, int64_t worldSeed,
                 uint64_t frameIndex);

    /// Single-chunk evaluation: thermal + gravity (SWAP) + transformation (WRITE).
    /// Returns gravity movement flag and transform count.
    EvaluateChunkResult evaluateChunk(ChunkCoord pos, std::mt19937& rng, BoundaryWriteQueue& boundaryWrites,
                                      std::vector<CellSwap>& cellSwaps, std::vector<SubRegionActivation>& activations);

    /// Legacy single-chunk thermal + transformation (no gravity). Kept for tests.
    void executeChunk(ChunkCoord pos, std::mt19937& rng);

    /// Legacy gravity-only evaluation. Kept for tests and replay.
    bool gravityEvaluation(ChunkCoord pos, std::mt19937& rng, BoundaryWriteQueue& boundaryWrites,
                           std::vector<CellSwap>& cellSwaps);

    Config& config() { return config_; }

    int totalTransforms() const { return totalTransforms_.load(std::memory_order_relaxed); }

  private:
    const WorldRuleEngine& rules_;
    const MaterialRegistry& registry_;
    SimulationGrid& grid_;
    const GhostCellManager& ghosts_;
    ChunkActivityTracker& tracker_;
    Config config_;
    std::atomic<int> totalTransforms_{0};

    void thermalKernel(ChunkCoord pos);
    void executeChunk(ChunkCoord pos, std::mt19937& rng, std::vector<SubRegionActivation>& activations);
    int ruleEvaluation(ChunkCoord pos, std::mt19937& rng, std::vector<SubRegionActivation>& activations);
    VoxelCell readCell(ChunkCoord pos, int lx, int ly, int lz) const;
    void writeSwap(ChunkCoord pos, int srcLx, int srcLy, int srcLz, int dstLx, int dstLy, int dstLz, VoxelCell srcCell,
                   VoxelCell dstCell, BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps) const;

    bool tryGravityMove(ChunkCoord pos, int lx, int ly, int lz, Phase phase, VoxelCell cell, std::mt19937& rng,
                        const std::vector<WorldRule>& gravityRules, std::vector<uint8_t>& moved,
                        BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps);
};

} // namespace recurse::simulation
