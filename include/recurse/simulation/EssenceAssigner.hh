#pragma once

#include "recurse/simulation/VoxelMaterial.hh"

namespace recurse {
class EssencePalette;
}

namespace recurse::simulation {

class MaterialRegistry;

/// Assign essence palette indices to all non-AIR voxels in a chunk buffer.
/// Looks up each material's baseEssence, applies small spatial hash noise,
/// and writes the quantized palette index into cell.spare (NOT essenceIdx,
/// which carries material identity during the MatterState migration).
void assignEssence(VoxelCell* buffer, int cx, int cy, int cz, const MaterialRegistry& materials,
                   recurse::EssencePalette& palette, int seed);

} // namespace recurse::simulation
