#pragma once

#include "fabric/core/Spatial.hh"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {

// Engine types imported from fabric:: namespace
namespace Space = fabric::Space;
using fabric::Vector4;

enum class MaterialType : uint8_t {
    Stone,
    Dirt,
    Grass,
    Wood,
    Metal,
    Water,
    Sand,
    Snow,
    Default
};

struct MaterialSoundSet {
    std::vector<std::string> footstepSounds;
    std::vector<std::string> impactSounds;
};

using EssenceColor = Vector4<float, Space::World>;

class MaterialSounds {
  public:
    MaterialSounds();
    explicit MaterialSounds(uint32_t seed);

    void registerMaterial(MaterialType type, MaterialSoundSet sounds);

    static MaterialType mapEssenceToMaterial(const EssenceColor& essence);

    std::string getFootstepSound(MaterialType type);
    std::string getImpactSound(MaterialType type);

    MaterialType detectSurfaceBelow(const recurse::simulation::SimulationGrid& grid, float x, float y, float z);

  private:
    std::string pickSound(const std::vector<std::string>& sounds, MaterialType type,
                          std::unordered_map<MaterialType, std::string>& lastPlayed);

    std::unordered_map<MaterialType, MaterialSoundSet> registry_;
    std::unordered_map<MaterialType, std::string> lastFootstep_;
    std::unordered_map<MaterialType, std::string> lastImpact_;
    std::mt19937 rng_;
};

} // namespace recurse
