#pragma once
#include "recurse/simulation/VoxelMaterial.hh"
#include <array>
#include <cassert>

namespace recurse::simulation {

/// Lookup table from MaterialId to MaterialDef. Fixed-size, no heap allocation.
class MaterialRegistry {
  public:
    MaterialRegistry();

    const MaterialDef& get(MaterialId id) const {
        assert(id < material_ids::COUNT && "MaterialId out of range");
        return materials_[id];
    }

    std::array<float, 4> terrainAppearanceColor(MaterialId id) const {
        return simulation::terrainAppearanceColor(get(id));
    }

    MaterialId count() const { return material_ids::COUNT; }

  private:
    std::array<MaterialDef, material_ids::COUNT> materials_{};
};

} // namespace recurse::simulation
