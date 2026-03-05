#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "recurse/world/TestWorldGenerator.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse::systems {

TerrainSystem::TerrainSystem() = default;
TerrainSystem::~TerrainSystem() = default;

void TerrainSystem::init(fabric::AppContext& /*ctx*/) {
    simGrid_ = std::make_unique<fabric::simulation::SimulationGrid>();
    worldGen_ = std::make_unique<FlatWorldGenerator>();

    // Generate initial 3x3x3 chunk region around origin
    for (int cz = -1; cz <= 1; ++cz) {
        for (int cy = -1; cy <= 1; ++cy) {
            for (int cx = -1; cx <= 1; ++cx) {
                worldGen_->generate(*simGrid_, cx, cy, cz);
            }
        }
    }
    simGrid_->advanceEpoch();

    FABRIC_LOG_INFO("TerrainSystem initialized with {} ({} chunks)", worldGen_->name(), simGrid_->allChunks().size());
}

void TerrainSystem::shutdown() {
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

} // namespace recurse::systems
