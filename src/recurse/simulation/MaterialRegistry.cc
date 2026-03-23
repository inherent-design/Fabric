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
    air.thermalConductivity = 255; // fast conductor (convection)

    // Stone: heavy static block
    auto& stone = materials_[material_ids::STONE];
    stone.moveType = MoveType::Static;
    stone.density = 200;
    stone.baseColor = 0xFF808080;
    stone.baseEssence[0] = 0.8f; // Order
    stone.baseEssence[1] = 0.1f; // Chaos
    stone.baseEssence[2] = 0.0f; // Life
    stone.baseEssence[3] = 0.1f; // Decay
    stone.meltPoint = 195;
    stone.boilPoint = 0;
    stone.thermalConductivity = 80; // moderate conductor

    // Dirt: medium static block
    auto& dirt = materials_[material_ids::DIRT];
    dirt.moveType = MoveType::Static;
    dirt.density = 150;
    dirt.baseColor = 0xFF8B6914;
    dirt.baseEssence[0] = 0.3f; // Order
    dirt.baseEssence[1] = 0.1f; // Chaos
    dirt.baseEssence[2] = 0.5f; // Life
    dirt.baseEssence[3] = 0.1f; // Decay
    dirt.meltPoint = 0;
    dirt.boilPoint = 0;
    dirt.thermalConductivity = 30; // poor conductor (insulator)

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
    sand.meltPoint = 179;
    sand.boilPoint = 0;
    sand.thermalConductivity = 50; // low conductor

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
    water.thermalConductivity = 150; // good conductor

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
    gravel.boilPoint = 0;
    gravel.thermalConductivity = 70; // moderate conductor
}

} // namespace recurse::simulation
