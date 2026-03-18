#pragma once

#include "fabric/world/ChunkCoord.hh"
#include "recurse/persistence/ChangeSource.hh"

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace recurse::simulation {
enum class ChunkFinalizationCause : uint8_t;
} // namespace recurse::simulation

namespace recurse {

enum class FunctionTargetKind : uint8_t {
    Voxel,
    Chunk,
    Region,
    Field,
};

struct VoxelTarget {
    int wx{0};
    int wy{0};
    int wz{0};
};

struct ChunkTarget {
    fabric::ChunkCoord chunk{};
};

enum class RegionShape : uint8_t {
    Box,
    ChunkSet,
    Frontier,
};

struct FunctionCostBudget {
    int64_t maxCells{-1};
    int64_t maxChunks{-1};
    int64_t maxFrontier{-1};
    float maxMillis{-1.0f};

    constexpr bool hasAnyLimit() const {
        return maxCells >= 0 || maxChunks >= 0 || maxFrontier >= 0 || maxMillis >= 0.0f;
    }
};

struct RegionTarget {
    VoxelTarget min{};
    VoxelTarget max{};
    RegionShape shape{RegionShape::Box};
    FunctionCostBudget budget{};
};

enum class FieldSamplingMode : uint8_t {
    Point,
    Neighborhood,
    Aggregate,
    Gradient,
};

struct FieldTarget {
    std::string_view fieldId{};
    RegionTarget region{};
    FieldSamplingMode samplingMode{FieldSamplingMode::Point};
};

using FunctionTarget = std::variant<VoxelTarget, ChunkTarget, RegionTarget, FieldTarget>;

constexpr FunctionTargetKind functionTargetKind(const VoxelTarget&) {
    return FunctionTargetKind::Voxel;
}
constexpr FunctionTargetKind functionTargetKind(const ChunkTarget&) {
    return FunctionTargetKind::Chunk;
}
constexpr FunctionTargetKind functionTargetKind(const RegionTarget&) {
    return FunctionTargetKind::Region;
}
constexpr FunctionTargetKind functionTargetKind(const FieldTarget&) {
    return FunctionTargetKind::Field;
}

inline FunctionTargetKind functionTargetKind(const FunctionTarget& target) {
    return std::visit([](const auto& value) { return functionTargetKind(value); }, target);
}

enum class FunctionCapability : uint32_t {
    PointRead = 1u << 0,
    NeighborhoodRead = 1u << 1,
    RegionRead = 1u << 2,
    FieldSample = 1u << 3,
    PointWrite = 1u << 4,
    RegionWrite = 1u << 5,
    ChunkReplace = 1u << 6,
    RequireMaterializedChunks = 1u << 7,
    AllowChunkStreaming = 1u << 8,
    WakeAffectedChunks = 1u << 9,
    EmitDetailedHistory = 1u << 10,
    EmitSummaryHistory = 1u << 11,
    NeedsBudget = 1u << 12,
};

using FunctionCapabilityMask = uint32_t;

constexpr FunctionCapabilityMask capabilityBit(FunctionCapability capability) {
    return static_cast<FunctionCapabilityMask>(capability);
}

template <typename... Caps> constexpr FunctionCapabilityMask capabilityMask(Caps... capabilities) {
    return (0u | ... | capabilityBit(capabilities));
}

constexpr bool hasCapability(FunctionCapabilityMask mask, FunctionCapability capability) {
    return (mask & capabilityBit(capability)) != 0u;
}

enum class FunctionHistoryMode : uint8_t {
    PerVoxelDelta,
    ChunkSummary,
    SnapshotOnly,
    DerivedNoHistory,
};

enum class FunctionCostClass : uint8_t {
    Constant,
    RayLinear,
    CellLinear,
    ChunkLinear,
    FrontierBudgeted,
};

struct FunctionOperationContract {
    FunctionTargetKind targetKind{FunctionTargetKind::Voxel};
    FunctionCapabilityMask capabilities{0};
    FunctionHistoryMode historyMode{FunctionHistoryMode::DerivedNoHistory};
    FunctionCostClass costClass{FunctionCostClass::Constant};
    FunctionCostBudget budget{};

    constexpr bool needsBudget() const {
        return hasCapability(capabilities, FunctionCapability::NeedsBudget) || budget.hasAnyLimit();
    }
};

struct VoxelChangeDetail {
    int vx{0};
    int vy{0};
    int vz{0};
    uint32_t oldCell{0};
    uint32_t newCell{0};
    int32_t playerId{0};
    ChangeSource source{ChangeSource::Place};
};

inline constexpr const char* K_WORLD_CHANGE_ENVELOPE_KEY = "changeEnvelope";

struct VoxelDelta {
    fabric::ChunkCoord chunk{};
    int vx{0};
    int vy{0};
    int vz{0};
    uint32_t oldCell{0};
    uint32_t newCell{0};
    int32_t playerId{0};
    ChangeSource source{ChangeSource::Place};
};

struct ChunkDelta {
    fabric::ChunkCoord chunk{};
    std::optional<simulation::ChunkFinalizationCause> finalizationCause{};
    bool dirty{true};
};

struct RegionSummary {
    RegionTarget region{};
    int64_t visitedCells{0};
    int64_t changedCells{0};
    int64_t touchedChunks{0};
    int64_t frontierPeak{0};
    float elapsedMillis{0.0f};
    bool complete{true};
};

struct WorldChangeCostSummary {
    FunctionCostClass costClass{FunctionCostClass::Constant};
    FunctionCostBudget budget{};
    int64_t visitedCells{0};
    int64_t changedCells{0};
    int64_t touchedChunks{0};
    int64_t frontierPeak{0};
    float elapsedMillis{0.0f};
    bool budgetExceeded{false};
};

struct WorldChangeEnvelope {
    ChangeSource source{ChangeSource::Place};
    FunctionTargetKind targetKind{FunctionTargetKind::Voxel};
    FunctionHistoryMode historyMode{FunctionHistoryMode::DerivedNoHistory};
    std::vector<fabric::ChunkCoord> touchedChunks{};
    WorldChangeCostSummary cost{};
    std::vector<VoxelDelta> voxelDeltas{};
    std::vector<ChunkDelta> chunkDeltas{};
    std::optional<RegionSummary> regionSummary{};

    bool hasDetailedHistory() const { return !voxelDeltas.empty(); }
    bool hasSummaryHistory() const { return !chunkDeltas.empty() || regionSummary.has_value(); }
};

inline VoxelDelta toVoxelDelta(fabric::ChunkCoord chunk, const VoxelChangeDetail& detail) {
    return VoxelDelta{chunk,          detail.vx,      detail.vy,       detail.vz,
                      detail.oldCell, detail.newCell, detail.playerId, detail.source};
}

inline WorldChangeEnvelope makeDetailedChangeEnvelope(fabric::ChunkCoord chunk,
                                                      const std::vector<VoxelChangeDetail>& details) {
    WorldChangeEnvelope envelope{};
    envelope.targetKind = FunctionTargetKind::Voxel;
    envelope.historyMode = FunctionHistoryMode::PerVoxelDelta;
    envelope.touchedChunks.push_back(chunk);
    envelope.cost.costClass = FunctionCostClass::Constant;
    envelope.cost.changedCells = static_cast<int64_t>(details.size());
    envelope.cost.touchedChunks = 1;
    if (!details.empty())
        envelope.source = details.front().source;
    envelope.voxelDeltas.reserve(details.size());
    for (const auto& detail : details)
        envelope.voxelDeltas.push_back(toVoxelDelta(chunk, detail));
    return envelope;
}

inline WorldChangeEnvelope
makeChunkSummaryChangeEnvelope(fabric::ChunkCoord chunk, ChangeSource source,
                               FunctionTargetKind targetKind = FunctionTargetKind::Chunk,
                               FunctionHistoryMode historyMode = FunctionHistoryMode::ChunkSummary,
                               FunctionCostClass costClass = FunctionCostClass::ChunkLinear,
                               std::optional<simulation::ChunkFinalizationCause> finalizationCause = std::nullopt) {
    WorldChangeEnvelope envelope{};
    envelope.source = source;
    envelope.targetKind = targetKind;
    envelope.historyMode = historyMode;
    envelope.touchedChunks.push_back(chunk);
    envelope.cost.costClass = costClass;
    envelope.cost.touchedChunks = 1;
    envelope.chunkDeltas.push_back(ChunkDelta{chunk, finalizationCause, true});
    return envelope;
}

} // namespace recurse
