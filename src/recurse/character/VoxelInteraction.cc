#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/VoxelConstants.hh"

#include <cstring>

using recurse::simulation::K_CHUNK_SHIFT;

namespace recurse {

VoxelInteraction::VoxelInteraction(SimulationGrid& grid) : grid_(grid) {}

InteractionResult VoxelInteraction::createMatter(const VoxelHit& hit, MaterialId materialId) {
    int x = hit.x + hit.nx;
    int y = hit.y + hit.ny;
    int z = hit.z + hit.nz;

    VoxelCell oldCell = grid_.readCell(x, y, z);
    VoxelCell newCell{materialId, 0, recurse::simulation::voxel_flags::UPDATED};
    grid_.writeCellImmediate(x, y, z, newCell);

    int cx = x >> K_CHUNK_SHIFT;
    int cy = y >> K_CHUNK_SHIFT;
    int cz = z >> K_CHUNK_SHIFT;

    static_assert(sizeof(VoxelCell) == sizeof(uint32_t));
    uint32_t oldCellU32 = 0, newCellU32 = 0;
    std::memcpy(&oldCellU32, &oldCell, sizeof(uint32_t));
    std::memcpy(&newCellU32, &newCell, sizeof(uint32_t));

    VoxelChangeDetail detail{};
    detail.vx = x & 0x1F;
    detail.vy = y & 0x1F;
    detail.vz = z & 0x1F;
    detail.oldCell = oldCellU32;
    detail.newCell = newCellU32;
    detail.playerId = 0;
    detail.source = ChangeSource::Place;

    return {true, x, y, z, cx, cy, cz, detail};
}

InteractionResult VoxelInteraction::destroyMatter(const VoxelHit& hit) {
    int x = hit.x;
    int y = hit.y;
    int z = hit.z;

    VoxelCell oldCell = grid_.readCell(x, y, z);
    VoxelCell newCell{recurse::simulation::material_ids::AIR, 0, recurse::simulation::voxel_flags::UPDATED};
    grid_.writeCellImmediate(x, y, z, newCell);

    int cx = x >> K_CHUNK_SHIFT;
    int cy = y >> K_CHUNK_SHIFT;
    int cz = z >> K_CHUNK_SHIFT;

    static_assert(sizeof(VoxelCell) == sizeof(uint32_t));
    uint32_t oldCellU32 = 0, newCellU32 = 0;
    std::memcpy(&oldCellU32, &oldCell, sizeof(uint32_t));
    std::memcpy(&newCellU32, &newCell, sizeof(uint32_t));

    VoxelChangeDetail detail{};
    detail.vx = x & 0x1F;
    detail.vy = y & 0x1F;
    detail.vz = z & 0x1F;
    detail.oldCell = oldCellU32;
    detail.newCell = newCellU32;
    detail.playerId = 0;
    detail.source = ChangeSource::Destroy;

    return {true, x, y, z, cx, cy, cz, detail};
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
