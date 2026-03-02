#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "recurse/world/CaveCarver.hh"
#include "recurse/world/ChunkedGrid.hh"
#include "recurse/world/TerrainGenerator.hh"

namespace recurse::systems {

TerrainSystem::~TerrainSystem() = default;

void TerrainSystem::init(fabric::AppContext& /*ctx*/) {
    TerrainConfig terrainConfig;
    terrainConfig.seed = 42;
    terrainConfig.frequency = 0.02f;
    terrainConfig.octaves = 4;
    terrainGen_ = std::make_unique<TerrainGenerator>(terrainConfig);

    CaveConfig caveConfig;
    caveConfig.seed = 42;
    caveCarver_ = std::make_unique<CaveCarver>(caveConfig);

    FABRIC_LOG_INFO("TerrainSystem initialized");
}

void TerrainSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    // Init-only system; terrain generation is driven by ChunkPipelineSystem
}

void TerrainSystem::configureDependencies() {
    // No dependencies; TerrainSystem is a root system
}

void TerrainSystem::generateChunkTerrain(int cx, int cy, int cz) {
    float x0 = static_cast<float>(cx * kChunkSize);
    float y0 = static_cast<float>(cy * kChunkSize);
    float z0 = static_cast<float>(cz * kChunkSize);
    float x1 = x0 + static_cast<float>(kChunkSize);
    float y1 = y0 + static_cast<float>(kChunkSize);
    float z1 = z0 + static_cast<float>(kChunkSize);
    fabric::AABB region(fabric::Vec3f(x0, y0, z0), fabric::Vec3f(x1, y1, z1));
    terrainGen_->generate(density_, essence_, region);
    caveCarver_->carve(density_, region);
}

} // namespace recurse::systems
