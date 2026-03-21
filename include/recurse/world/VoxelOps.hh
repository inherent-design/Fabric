#pragma once

#include "fabric/fx/SpatialDataOp.hh"
#include "recurse/persistence/ChangeSource.hh"
#include "recurse/world/FunctionContracts.hh"

namespace recurse::simulation {
struct VoxelCell;
} // namespace recurse::simulation

namespace recurse::ops {

/// Write a voxel cell with game-specific metadata.
/// Extends SpatialWrite<VoxelCell> with change tracking fields.
struct VoxelWrite {
    int wx, wy, wz;
    simulation::VoxelCell value;
    ChangeSource source;
    float stabilityCost{0.0f};
    float essenceContribution{0.0f};

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
    using ContractTarget = VoxelTarget;
    static constexpr FunctionOperationContract K_CONTRACT{
        FunctionTargetKind::Voxel,
        capabilityMask(FunctionCapability::PointWrite, FunctionCapability::WakeAffectedChunks,
                       FunctionCapability::EmitDetailedHistory),
        FunctionHistoryMode::PerVoxelDelta,
        FunctionCostClass::Constant,
        {},
    };
};

} // namespace recurse::ops
