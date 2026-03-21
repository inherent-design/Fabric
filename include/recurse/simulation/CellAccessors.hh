#pragma once

#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/MatterState.hh"
#include "recurse/simulation/ProjectionRuleTable.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <concepts>

namespace recurse::simulation {

/// True when the cell is occupied (not Empty phase).
constexpr bool isOccupied(VoxelCell cell) {
    return cell.phase() != Phase::Empty;
}

/// True when the cell is empty.
constexpr bool isEmpty(VoxelCell cell) {
    return cell.phase() == Phase::Empty;
}

/// Returns the cell phase as MoveType. Registry parameter accepted for concept
/// satisfaction but unused; VoxelCell carries its phase directly.
inline MoveType cellPhase(const MaterialRegistry& /*registry*/, VoxelCell cell) {
    switch (cell.phase()) {
        case Phase::Solid:
            return MoveType::Static;
        case Phase::Powder:
            return MoveType::Powder;
        case Phase::Liquid:
            return MoveType::Liquid;
        case Phase::Gas:
            return MoveType::Gas;
        default:
            return MoveType::Static;
    }
}

/// Displacement check for VoxelCell. Uses displacementRank directly.
inline bool canDisplace(const MaterialRegistry& /*registry*/, VoxelCell mover, VoxelCell target) {
    if (isEmpty(target))
        return true;
    if (target.phase() == Phase::Solid)
        return false;
    return mover.displacementRank > target.displacementRank;
}

// -- MatterState overloads --------------------------------------------------

/// True when the MatterState cell is occupied (not Empty phase).
constexpr bool isOccupied(MatterState cell) {
    return cell.phase() != Phase::Empty;
}

/// True when the MatterState cell is empty.
constexpr bool isEmpty(MatterState cell) {
    return cell.phase() == Phase::Empty;
}

/// Returns the cell phase from MatterState directly. Registry parameter is
/// accepted for concept satisfaction but unused; MatterState carries its phase.
inline MoveType cellPhase(const MaterialRegistry& /*registry*/, MatterState cell) {
    switch (cell.phase()) {
        case Phase::Solid:
            return MoveType::Static;
        case Phase::Powder:
            return MoveType::Powder;
        case Phase::Liquid:
            return MoveType::Liquid;
        case Phase::Gas:
            return MoveType::Gas;
        default:
            return MoveType::Static;
    }
}

/// Displacement check for MatterState cells. Uses displacementRank directly.
inline bool canDisplace(const MaterialRegistry& /*registry*/, MatterState mover, MatterState target) {
    if (isEmpty(target))
        return true;
    if (target.phase() == Phase::Solid)
        return false;
    return mover.displacementRank > target.displacementRank;
}

// -- Cell factory functions ---------------------------------------------------

/// Derive Phase from MoveType. Used during MaterialId -> MatterState construction.
constexpr Phase phaseFromMoveType(MoveType mt) {
    switch (mt) {
        case MoveType::Static:
            return Phase::Solid;
        case MoveType::Powder:
            return Phase::Powder;
        case MoveType::Liquid:
            return Phase::Liquid;
        case MoveType::Gas:
            return Phase::Gas;
        default:
            return Phase::Solid;
    }
}

/// Construct a cell from raw fields.
constexpr VoxelCell makeCell(uint8_t essenceIdx, Phase phase, uint8_t displacementRank = 0, uint8_t flags = 0) {
    VoxelCell cell;
    cell.essenceIdx = essenceIdx;
    cell.displacementRank = displacementRank;
    cell.setPhase(phase);
    cell.setFlags(flags);
    return cell;
}

/// Construct an empty (air) cell.
constexpr VoxelCell emptyCell() {
    return VoxelCell{};
}

/// Construct a VoxelCell from a MaterialId using the MaterialRegistry.
/// During migration: essenceIdx == materialId (1:1 mapping).
inline VoxelCell makeCellFromMaterial(MaterialId id, const MaterialRegistry& registry, uint8_t flags = 0) {
    VoxelCell cell;
    cell.essenceIdx = static_cast<uint8_t>(id);
    const auto& def = registry.get(id);
    cell.displacementRank = def.density;
    cell.setPhase(id == material_ids::AIR ? Phase::Empty : phaseFromMoveType(def.moveType));
    cell.setFlags(flags);
    return cell;
}

/// Migration helper: construct a VoxelCell from a MaterialId using known
/// material properties. Avoids requiring a MaterialRegistry instance.
/// Only valid during migration when essenceIdx == materialId.
inline VoxelCell cellForMaterial(MaterialId id) {
    switch (id) {
        case material_ids::AIR:
            return VoxelCell{};
        case material_ids::STONE:
            return makeCell(1, Phase::Solid, 200);
        case material_ids::DIRT:
            return makeCell(2, Phase::Solid, 150);
        case material_ids::SAND:
            return makeCell(3, Phase::Powder, 130);
        case material_ids::WATER:
            return makeCell(4, Phase::Liquid, 100);
        case material_ids::GRAVEL:
            return makeCell(5, Phase::Powder, 170);
        default:
            return makeCell(static_cast<uint8_t>(id), Phase::Solid, 128);
    }
}

// -- Cell concepts ----------------------------------------------------------

/// A cell type that supports occupancy queries without external context.
template <typename T>
concept CellQuery = requires(T cell) {
    { isOccupied(cell) } -> std::convertible_to<bool>;
    { isEmpty(cell) } -> std::convertible_to<bool>;
};

/// A cell type that supports semantic queries requiring registry context.
template <typename T>
concept SemanticQuery = CellQuery<T> && requires(const MaterialRegistry& reg, T cell, T other) {
    { cellPhase(reg, cell) } -> std::convertible_to<MoveType>;
    { canDisplace(reg, cell, other) } -> std::convertible_to<bool>;
};

static_assert(CellQuery<VoxelCell>, "VoxelCell must satisfy CellQuery");
static_assert(CellQuery<MatterState>, "MatterState must satisfy CellQuery");
static_assert(SemanticQuery<VoxelCell>, "VoxelCell must satisfy SemanticQuery");
static_assert(SemanticQuery<MatterState>, "MatterState must satisfy SemanticQuery");

// ---------------------------------------------------------------------------

/// Extract the raw material id from a cell. During migration essenceIdx maps
/// 1:1 to MaterialId. This is the single quarantine point for the mapping.
constexpr MaterialId cellMaterialId(VoxelCell cell) {
    return static_cast<MaterialId>(cell.essenceIdx);
}

/// Semantic priority for LOD material reduction. Higher values win tiebreaks
/// during 2x2x2 downsampling. The body stays as-is; quarantined here so
/// consumers do not switch on raw materialId constants directly.
inline int materialSemanticPriority(uint16_t materialId) {
    switch (materialId) {
        case material_ids::SAND:
        case material_ids::GRAVEL:
            return 4;
        case material_ids::WATER:
            return 3;
        case material_ids::DIRT:
            return 2;
        case material_ids::STONE:
            return 1;
        default:
            return 0;
    }
}

/// Merge key for greedy meshing. Adjacent faces with equal keys are merged
/// into a single quad. Initially maps to materialId; Wave 4 swaps to a
/// visual-equivalence hash derived from MatterState fields.
using MergeKey = uint16_t;

/// Sentinel value indicating an empty (unmergeable) face slot in the mask.
inline constexpr MergeKey K_MERGE_KEY_EMPTY = static_cast<MergeKey>(material_ids::AIR);

/// Extract the merge key from a cell for the greedy mesher mask array.
inline MergeKey mergeKey(VoxelCell cell) {
    return static_cast<MergeKey>(cell.essenceIdx);
}

/// True when two adjacent face slots can be merged into a single greedy quad.
inline bool canMergeQuads(MergeKey a, MergeKey b) {
    return a == b;
}

// -- Projection-aware accessors (MatterState) --------------------------------

/// Derive MaterialId from MatterState during migration.
/// Uses essenceIdx directly (1:1 mapping with MaterialId during migration).
constexpr MaterialId cellMaterialId(MatterState cell) {
    return static_cast<MaterialId>(cell.essenceIdx);
}

/// Merge key for MatterState. Uses essenceIdx as visual identity proxy.
inline MergeKey mergeKey(MatterState cell) {
    return static_cast<MergeKey>(cell.essenceIdx);
}

/// Semantic priority for LOD reduction from MatterState.
/// Delegates to the existing materialId-based function via projection.
inline int materialSemanticPriority(MatterState cell) {
    return materialSemanticPriority(static_cast<uint16_t>(cell.essenceIdx));
}

/// Derive MaterialId from MatterState via ProjectionRuleTable.
/// Future-proof: uses table lookup instead of direct essenceIdx cast.
/// During migration this returns the same result as the non-table version.
inline MaterialId cellMaterialId(const ProjectionRuleTable& /*table*/, MatterState cell) {
    // During migration: table.lookup maps essenceIdx -> ProjectedMaterial
    // which has baseColor/moveType/density but not materialId directly.
    // For now essenceIdx IS the materialId.
    return static_cast<MaterialId>(cell.essenceIdx);
}

} // namespace recurse::simulation
