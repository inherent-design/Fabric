#include "recurse/gameplay/VoxelInteraction.hh"

namespace recurse {

VoxelInteraction::VoxelInteraction(SimulationGrid& grid, EventDispatcher& dispatcher)
    : grid_(grid), dispatcher_(dispatcher) {}

InteractionResult VoxelInteraction::createMatter(const VoxelHit& hit, MaterialId materialId) {
    // Place adjacent to hit face via normal
    int x = hit.x + hit.nx;
    int y = hit.y + hit.ny;
    int z = hit.z + hit.nz;

    grid_.writeCellImmediate(x, y, z, VoxelCell{materialId, 128, fabric::simulation::voxel_flags::UPDATED});

    int cx = x >> kChunkShift;
    int cy = y >> kChunkShift;
    int cz = z >> kChunkShift;
    emitVoxelChanged(dispatcher_, cx, cy, cz);

    return {true, x, y, z, cx, cy, cz};
}

InteractionResult VoxelInteraction::destroyMatter(const VoxelHit& hit) {
    int x = hit.x;
    int y = hit.y;
    int z = hit.z;

    grid_.writeCellImmediate(
        x, y, z, VoxelCell{fabric::simulation::material_ids::AIR, 128, fabric::simulation::voxel_flags::UPDATED});

    int cx = x >> kChunkShift;
    int cy = y >> kChunkShift;
    int cz = z >> kChunkShift;
    emitVoxelChanged(dispatcher_, cx, cy, cz);

    return {true, x, y, z, cx, cy, cz};
}

InteractionResult VoxelInteraction::createMatterAt(float ox, float oy, float oz, float dx, float dy, float dz,
                                                   MaterialId materialId, float maxDistance) {
    auto hit = castRay(grid_, ox, oy, oz, dx, dy, dz, maxDistance);
    if (!hit.has_value())
        return {false, 0, 0, 0, 0, 0, 0};
    return createMatter(*hit, materialId);
}

InteractionResult VoxelInteraction::destroyMatterAt(float ox, float oy, float oz, float dx, float dy, float dz,
                                                    float maxDistance) {
    auto hit = castRay(grid_, ox, oy, oz, dx, dy, dz, maxDistance);
    if (!hit.has_value())
        return {false, 0, 0, 0, 0, 0, 0};
    return destroyMatter(*hit);
}

bool VoxelInteraction::wouldOverlap(int vx, int vy, int vz, const fabric::AABB& playerBounds) {
    fabric::AABB voxelBounds(
        fabric::Vec3f(static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)),
        fabric::Vec3f(static_cast<float>(vx + 1), static_cast<float>(vy + 1), static_cast<float>(vz + 1)));
    return voxelBounds.intersects(playerBounds);
}

} // namespace recurse
