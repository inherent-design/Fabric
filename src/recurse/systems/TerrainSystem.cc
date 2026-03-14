#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/log/Log.hh"
#include "recurse/world/TestWorldGenerator.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse::systems {

TerrainSystem::TerrainSystem() = default;
TerrainSystem::~TerrainSystem() = default;

void TerrainSystem::doInit(fabric::AppContext& /*ctx*/) {
    worldGen_ = std::make_unique<FlatWorldGenerator>();
    FABRIC_LOG_INFO("TerrainSystem initialized");
}

void TerrainSystem::doShutdown() {
    worldGen_.reset();
    FABRIC_LOG_INFO("TerrainSystem shutdown");
}

void TerrainSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    // Init-only system: no per-tick work
}

void TerrainSystem::configureDependencies() {
    // Root system: no dependencies
}

void TerrainSystem::setWorldGenerator(std::unique_ptr<WorldGenerator> gen) {
    worldGen_ = std::move(gen);
}

} // namespace recurse::systems
