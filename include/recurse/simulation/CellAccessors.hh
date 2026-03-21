#pragma once

#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/VoxelMaterial.hh"

namespace recurse::simulation {

/// True when the cell contains a non-AIR material.
inline constexpr bool isOccupied(const VoxelCell& cell) {
    return cell.materialId != material_ids::AIR;
}

/// True when the cell is AIR (empty).
inline constexpr bool isEmpty(const VoxelCell& cell) {
    return cell.materialId == material_ids::AIR;
}

/// Returns the cell move phase from the material registry.
inline MoveType cellPhase(const MaterialRegistry& registry, VoxelCell cell) {
    return registry.get(cell.materialId).moveType;
}

/// True when mover can displace target (empty, non-static, or lower density).
inline bool canDisplace(const MaterialRegistry& registry, VoxelCell mover, VoxelCell target) {
    if (isEmpty(target))
        return true;
    if (cellPhase(registry, target) == MoveType::Static)
        return false;
    const auto& targetDef = registry.get(target.materialId);
    const auto& moverDef = registry.get(mover.materialId);
    return moverDef.density > targetDef.density;
}

} // namespace recurse::simulation
