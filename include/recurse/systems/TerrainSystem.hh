#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/render/Geometry.hh"
#include <memory>

namespace recurse {
class WorldGenerator;
} // namespace recurse

namespace recurse::systems {

/// Owns WorldGenerator. Init-only system.
class TerrainSystem : public fabric::System<TerrainSystem> {
  public:
    TerrainSystem();
    ~TerrainSystem() override;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;
    void configureDependencies() override;

    WorldGenerator& worldGenerator() { return *worldGen_; }
    void setWorldGenerator(std::unique_ptr<WorldGenerator> gen);

  private:
    std::unique_ptr<WorldGenerator> worldGen_;
};

} // namespace recurse::systems
