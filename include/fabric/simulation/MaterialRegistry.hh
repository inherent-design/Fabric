#pragma once
#include "fabric/simulation/VoxelMaterial.hh"
#include <array>
#include <cassert>

namespace fabric::simulation {

/// Lookup table from MaterialId to MaterialDef. Fixed-size, no heap allocation.
class MaterialRegistry {
  public:
    MaterialRegistry();

    const MaterialDef& get(MaterialId id) const {
        assert(id < MaterialIds::Count && "MaterialId out of range");
        return materials_[id];
    }

    MaterialId count() const { return MaterialIds::Count; }

  private:
    std::array<MaterialDef, MaterialIds::Count> materials_{};
};

} // namespace fabric::simulation
