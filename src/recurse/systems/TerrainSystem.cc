#include "recurse/systems/TerrainSystem.hh"
#include "recurse/world/DensitySync.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "recurse/world/TestWorldGenerator.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse::systems {

TerrainSystem::TerrainSystem() = default;
TerrainSystem::~TerrainSystem() = default;

void TerrainSystem::doInit(fabric::AppContext& /*ctx*/) {
    simGrid_ = std::make_unique<fabric::simulation::SimulationGrid>();
    // Create default generator but don't generate terrain yet
    // This allows MainMenuSystem to swap the generator before world generation
    worldGen_ = std::make_unique<FlatWorldGenerator>();
    FABRIC_LOG_INFO("TerrainSystem initialized (awaiting generateInitialWorld call)");
}

void TerrainSystem::generateInitialWorld() {
    if (worldGenerated_) {
        FABRIC_LOG_WARN("TerrainSystem: World already generated, skipping");
        return;
    }

    if (!worldGen_) {
        worldGen_ = std::make_unique<FlatWorldGenerator>();
        FABRIC_LOG_INFO("TerrainSystem: Using default FlatWorldGenerator");
    }

    // Generate initial 5x3x5 chunk region around origin
    for (int cz = -2; cz <= 2; ++cz) {
        for (int cy = -1; cy <= 1; ++cy) {
            for (int cx = -2; cx <= 2; ++cx) {
                worldGen_->generate(*simGrid_, cx, cy, cz);
            }
        }
    }
    simGrid_->advanceEpoch();

    // Sync density field from simulation grid for collision detection
    syncDensityFromSimulation();

    worldGenerated_ = true;
    FABRIC_LOG_INFO("TerrainSystem: World generated with {} ({} chunks)", worldGen_->name(),
                    simGrid_->allChunks().size());
}

void TerrainSystem::doShutdown() {
    worldGen_.reset();
    simGrid_.reset();
}

void TerrainSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    // Init-only system: no per-tick work
}

void TerrainSystem::configureDependencies() {
    // Root system: no dependencies
}

void TerrainSystem::generateChunk(int cx, int cy, int cz) {
    if (!worldGen_ || !simGrid_)
        return;
    worldGen_->generate(*simGrid_, cx, cy, cz);
    simGrid_->advanceEpoch();

    // Sync density for the new chunk
    syncDensityFromSimulation();
}

void TerrainSystem::syncDensityFromSimulation() {
    if (simGrid_)
        syncDensityFromGrid(*simGrid_);
}

void TerrainSystem::syncDensityFromGrid(const fabric::simulation::SimulationGrid& grid) {
    for (const auto& [cx, cy, cz] : grid.allChunks()) {
        syncChunkDensity(grid, density_.grid(), cx, cy, cz);
    }
}

fabric::simulation::SimulationGrid& TerrainSystem::simulationGrid() {
    return *simGrid_;
}

const fabric::simulation::SimulationGrid& TerrainSystem::simulationGrid() const {
    return *simGrid_;
}

void TerrainSystem::setWorldGenerator(std::unique_ptr<WorldGenerator> gen) {
    worldGen_ = std::move(gen);
}

void TerrainSystem::resetWorld() {
    // Clear simulation grid
    if (simGrid_)
        simGrid_->clear();

    // Clear density field (deprecated but used for collision)
    density_.grid().clear();

    // Reset generation flag
    worldGenerated_ = false;

    FABRIC_LOG_INFO("TerrainSystem: World reset (grid cleared, flags reset)");
}

} // namespace recurse::systems
