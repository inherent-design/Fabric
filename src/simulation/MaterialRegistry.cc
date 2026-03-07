#include "fabric/simulation/MaterialRegistry.hh"

namespace fabric::simulation {

MaterialRegistry::MaterialRegistry() {
    // Air: transparent, no physics
    auto& air = materials_[material_ids::AIR];
    air.moveType = MoveType::Static;
    air.density = 0;
    air.baseColor = 0x00000000;

    // Stone: heavy static block
    auto& stone = materials_[material_ids::STONE];
    stone.moveType = MoveType::Static;
    stone.density = 200;
    stone.baseColor = 0xFF808080;

    // Dirt: medium static block
    auto& dirt = materials_[material_ids::DIRT];
    dirt.moveType = MoveType::Static;
    dirt.density = 150;
    dirt.baseColor = 0xFF8B6914;

    // Sand: powder, falls and cascades
    auto& sand = materials_[material_ids::SAND];
    sand.moveType = MoveType::Powder;
    sand.density = 130;
    sand.viscosity = 0;
    sand.dispersionRate = 0;
    sand.baseColor = 0xFFC2B280;

    // Water: liquid, flows horizontally
    auto& water = materials_[material_ids::WATER];
    water.moveType = MoveType::Liquid;
    water.density = 100;
    water.viscosity = 10;
    water.dispersionRate = 3;
    water.baseColor = 0xFF4040C0;

    // Gravel: powder, heavier than sand
    auto& gravel = materials_[material_ids::GRAVEL];
    gravel.moveType = MoveType::Powder;
    gravel.density = 170;
    gravel.viscosity = 0;
    gravel.dispersionRate = 0;
    gravel.baseColor = 0xFF606060;
}

} // namespace fabric::simulation
