#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/VoxelConstants.hh"

using recurse::simulation::K_CHUNK_SHIFT;

namespace recurse {

namespace {

InteractionResult makeInteractionResult(int x, int y, int z, VoxelCell newCell, ChangeSource source) {
    return {true, x, y, z, x >> K_CHUNK_SHIFT, y >> K_CHUNK_SHIFT, z >> K_CHUNK_SHIFT, newCell, source, 0};
}

} // namespace

VoxelInteraction::VoxelInteraction(SimulationGrid& grid) : grid_(grid) {}

InteractionResult VoxelInteraction::createMatter(const VoxelHit& hit, MaterialId materialId) {
    int x = hit.x + hit.nx;
    int y = hit.y + hit.ny;
    int z = hit.z + hit.nz;
    VoxelCell newCell{materialId, 0, recurse::simulation::voxel_flags::UPDATED};
    return makeInteractionResult(x, y, z, newCell, ChangeSource::Place);
}

InteractionResult VoxelInteraction::destroyMatter(const VoxelHit& hit) {
    int x = hit.x;
    int y = hit.y;
    int z = hit.z;
    VoxelCell newCell{recurse::simulation::material_ids::AIR, 0, recurse::simulation::voxel_flags::UPDATED};
    return makeInteractionResult(x, y, z, newCell, ChangeSource::Destroy);
}

InteractionResult VoxelInteraction::createMatterAt(float ox, float oy, float oz, float dx, float dy, float dz,
                                                   MaterialId materialId, float maxDistance) {
    auto hit = castRay(grid_, ox, oy, oz, dx, dy, dz, maxDistance);
    if (!hit.has_value())
        return {};
    return createMatter(*hit, materialId);
}

InteractionResult VoxelInteraction::destroyMatterAt(float ox, float oy, float oz, float dx, float dy, float dz,
                                                    float maxDistance) {
    auto hit = castRay(grid_, ox, oy, oz, dx, dy, dz, maxDistance);
    if (!hit.has_value())
        return {};
    return destroyMatter(*hit);
}

bool VoxelInteraction::wouldOverlap(int vx, int vy, int vz, const fabric::AABB& playerBounds) {
    fabric::AABB voxelBounds(
        fabric::Vec3f(static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)),
        fabric::Vec3f(static_cast<float>(vx + 1), static_cast<float>(vy + 1), static_cast<float>(vz + 1)));
    return voxelBounds.intersects(playerBounds);
}

} // namespace recurse
