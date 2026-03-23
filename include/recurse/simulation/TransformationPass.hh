#pragma once
#include "fabric/platform/JobScheduler.hh"
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

struct ActiveChunkEntry; // forward declare

struct SubRegionActivation {
    ChunkCoord pos;
    int lx, ly, lz;
};

class TransformationPass {
  public:
    struct Config {
        float diffusionRate = 0.25f;
        int maxTransformsPerChunk = K_MAX_TRANSFORMS_PER_CHUNK;
    };

    TransformationPass(const WorldRuleEngine& rules, const MaterialRegistry& registry, SimulationGrid& grid,
                       const GhostCellManager& ghosts, ChunkActivityTracker& tracker);

    /// Execute Phase 3c across all active chunks.
    void execute(const std::vector<ActiveChunkEntry>& active, fabric::JobScheduler& scheduler, int64_t worldSeed,
                 uint64_t frameIndex);

    /// Per-chunk execution (single-threaded; flushes activations to tracker directly).
    void executeChunk(ChunkCoord pos, std::mt19937& rng);

    Config& config() { return config_; }

    /// Stats from last execute() call.
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
};

} // namespace recurse::simulation
