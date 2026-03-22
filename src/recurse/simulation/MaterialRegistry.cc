#include "recurse/simulation/MaterialRegistry.hh"

namespace recurse::simulation {

MaterialRegistry::MaterialRegistry() {
    // Air: transparent, no physics
    auto& air = materials_[material_ids::AIR];
    air.moveType = MoveType::Static;
    air.density = 0;
    air.baseColor = 0x00000000;
    air.meltPoint = 0;
    air.boilPoint = 0;
    air.thermalConductivity = 5; // 0.02 scaled to uint8

    // Stone: heavy static block
    auto& stone = materials_[material_ids::STONE];
    stone.moveType = MoveType::Static;
    stone.density = 200;
    stone.baseColor = 0xFF808080;
    stone.baseEssence[0] = 0.8f; // Order
    stone.baseEssence[1] = 0.1f; // Chaos
    stone.baseEssence[2] = 0.0f; // Life
    stone.baseEssence[3] = 0.1f; // Decay
    stone.meltPoint = 200;
    stone.boilPoint = 255;
    stone.thermalConductivity = 102; // 0.4 scaled to uint8

    // Dirt: medium static block
    auto& dirt = materials_[material_ids::DIRT];
    dirt.moveType = MoveType::Static;
    dirt.density = 150;
    dirt.baseColor = 0xFF8B6914;
    dirt.baseEssence[0] = 0.3f; // Order
    dirt.baseEssence[1] = 0.1f; // Chaos
    dirt.baseEssence[2] = 0.5f; // Life
    dirt.baseEssence[3] = 0.1f; // Decay
    dirt.meltPoint = 180;
    dirt.boilPoint = 240;
    dirt.thermalConductivity = 38; // 0.15 scaled to uint8

    // Sand: powder, falls and cascades
    auto& sand = materials_[material_ids::SAND];
    sand.moveType = MoveType::Powder;
    sand.density = 130;
    sand.viscosity = 0;
    sand.dispersionRate = 0;
    sand.baseColor = 0xFFC2B280;
    sand.baseEssence[0] = 0.4f; // Order
    sand.baseEssence[1] = 0.3f; // Chaos
    sand.baseEssence[2] = 0.1f; // Life
    sand.baseEssence[3] = 0.2f; // Decay
    sand.meltPoint = 220;
    sand.boilPoint = 255;
    sand.thermalConductivity = 64; // 0.25 scaled to uint8

    // Water: liquid, flows horizontally
    auto& water = materials_[material_ids::WATER];
    water.moveType = MoveType::Liquid;
    water.density = 100;
    water.viscosity = 10;
    water.dispersionRate = 3;
    water.baseColor = 0xFF4040C0;
    water.baseEssence[0] = 0.2f; // Order
    water.baseEssence[1] = 0.2f; // Chaos
    water.baseEssence[2] = 0.4f; // Life
    water.baseEssence[3] = 0.2f; // Decay
    water.meltPoint = 91;
    water.boilPoint = 124;
    water.thermalConductivity = 153; // 0.6 scaled to uint8

    // Gravel: powder, heavier than sand
    auto& gravel = materials_[material_ids::GRAVEL];
    gravel.moveType = MoveType::Powder;
    gravel.density = 170;
    gravel.viscosity = 0;
    gravel.dispersionRate = 0;
    gravel.baseColor = 0xFF606060;
    gravel.baseEssence[0] = 0.5f; // Order
    gravel.baseEssence[1] = 0.2f; // Chaos
    gravel.baseEssence[2] = 0.0f; // Life
    gravel.baseEssence[3] = 0.3f; // Decay
    gravel.meltPoint = 190;
    gravel.boilPoint = 250;
    gravel.thermalConductivity = 77; // 0.3 scaled to uint8
}

} // namespace recurse::simulation
