#pragma once

#include "recurse/components/EssenceTypes.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/world/EssencePalette.hh"

#include <optional>

namespace recurse::simulation {

using EssenceVector = fabric::Vector4<float, fabric::Space::World>;

struct EssenceValue {
    float order{0.0f};
    float chaos{0.0f};
    float life{0.0f};
    float decay{0.0f};

    static EssenceValue fromArray(const float* arr) { return {arr[0], arr[1], arr[2], arr[3]}; }

    static EssenceValue fromArray(const std::array<float, 4>& arr) { return {arr[0], arr[1], arr[2], arr[3]}; }

    static EssenceValue fromVector(const EssenceVector& value) { return {value.x, value.y, value.z, value.w}; }

    static EssenceValue fromSemantic(const recurse::Essence& value) {
        return {value.order, value.chaos, value.life, value.decay};
    }

    static EssenceValue fromMaterialDef(const MaterialDef& def) { return fromArray(def.baseEssence); }

    std::array<float, 4> toArray() const { return {order, chaos, life, decay}; }

    EssenceVector toVector() const { return {order, chaos, life, decay}; }

    recurse::Essence toSemanticEssence() const { return {order, chaos, life, decay}; }

    recurse::EssenceType dominant(float threshold = 0.3f) const { return toSemanticEssence().dominant(threshold); }
};

struct TerrainAppearanceProjection {
    std::array<float, 4> color{0.0f, 0.0f, 0.0f, 0.0f};
};

struct VoxelOccupancyProjection {
    bool occupied{false};
    bool blocksRaycast{false};
    float density{0.0f};
};

inline const char* materialDisplayName(MaterialId id) {
    switch (id) {
        case material_ids::AIR:
            return "Air";
        case material_ids::STONE:
            return "Stone";
        case material_ids::DIRT:
            return "Dirt";
        case material_ids::SAND:
            return "Sand";
        case material_ids::WATER:
            return "Water";
        case material_ids::GRAVEL:
            return "Gravel";
        default:
            return "Unknown";
    }
}

inline const char* moveTypeLabel(MoveType moveType) {
    switch (moveType) {
        case MoveType::Static:
            return "Static";
        case MoveType::Powder:
            return "Powder";
        case MoveType::Liquid:
            return "Liquid";
        case MoveType::Gas:
            return "Gas";
        default:
            return "Unknown";
    }
}

struct MaterialSemanticView {
    MaterialId materialId{material_ids::AIR};
    const MaterialDef* material{nullptr};
    const char* displayName{"Air"};
    MoveType moveType{MoveType::Static};
    const char* moveTypeName{"Static"};
    uint8_t materialDensity{0};
    uint8_t viscosity{0};
    uint8_t dispersionRate{0};
    EssenceValue intrinsicEssence{};
    TerrainAppearanceProjection terrainAppearance{};
    VoxelOccupancyProjection occupancy{};
};

struct SampledEssenceResolution {
    uint8_t index{0};
    bool hasPalette{false};
    bool inRange{false};
    std::optional<EssenceValue> value{};
};

struct ResolvedVoxelSemantics {
    MaterialSemanticView material{};
    SampledEssenceResolution sampledEssence{};
};

class MaterialSemanticRegistry {
  public:
    explicit MaterialSemanticRegistry(const MaterialRegistry& materials) : materials_(&materials) {}

    MaterialSemanticView view(MaterialId id) const {
        const auto& def = materials_->get(id);

        MaterialSemanticView result{};
        result.materialId = id;
        result.material = &def;
        result.displayName = materialDisplayName(id);
        result.moveType = def.moveType;
        result.moveTypeName = moveTypeLabel(def.moveType);
        result.materialDensity = def.density;
        result.viscosity = def.viscosity;
        result.dispersionRate = def.dispersionRate;
        result.intrinsicEssence = EssenceValue::fromMaterialDef(def);
        result.terrainAppearance.color = materials_->terrainAppearanceColor(id);
        result.occupancy.occupied = (id != material_ids::AIR);
        result.occupancy.blocksRaycast = (id != material_ids::AIR);
        result.occupancy.density = result.occupancy.occupied ? 1.0f : 0.0f;
        return result;
    }

    ResolvedVoxelSemantics resolve(const VoxelCell& cell, const recurse::EssencePalette* palette = nullptr) const {
        ResolvedVoxelSemantics result{};
        result.material = view(cell.materialId);
        result.sampledEssence.index = cell.essenceIdx;
        result.sampledEssence.hasPalette = (palette != nullptr);
        if (palette != nullptr && cell.essenceIdx < palette->paletteSize()) {
            result.sampledEssence.inRange = true;
            result.sampledEssence.value = EssenceValue::fromVector(palette->lookup(cell.essenceIdx));
        }
        return result;
    }

    ResolvedVoxelSemantics resolve(const VoxelCell& cell, const recurse::EssencePalette& palette) const {
        return resolve(cell, &palette);
    }

  private:
    const MaterialRegistry* materials_;
};

} // namespace recurse::simulation
