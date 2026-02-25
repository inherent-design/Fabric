#include "fabric/core/VoxelInteraction.hh"
#include "fabric/core/ChunkMeshManager.hh"

namespace fabric {

VoxelInteraction::VoxelInteraction(DensityField& density, EssenceField& essence, EventDispatcher& dispatcher)
    : density_(density), essence_(essence), dispatcher_(dispatcher) {}

InteractionResult VoxelInteraction::createMatter(const VoxelHit& hit, float density,
                                                 const Vector4<float, Space::World>& essenceColor) {

    // Place adjacent to hit face via normal
    int x = hit.x + hit.nx;
    int y = hit.y + hit.ny;
    int z = hit.z + hit.nz;

    density_.write(x, y, z, density);
    essence_.write(x, y, z, essenceColor);

    int cx = x >> kChunkShift;
    int cy = y >> kChunkShift;
    int cz = z >> kChunkShift;
    ChunkMeshManager::emitVoxelChanged(dispatcher_, cx, cy, cz);

    return {true, x, y, z, cx, cy, cz};
}

InteractionResult VoxelInteraction::destroyMatter(const VoxelHit& hit) {
    int x = hit.x;
    int y = hit.y;
    int z = hit.z;

    density_.write(x, y, z, 0.0f);

    int cx = x >> kChunkShift;
    int cy = y >> kChunkShift;
    int cz = z >> kChunkShift;
    ChunkMeshManager::emitVoxelChanged(dispatcher_, cx, cy, cz);

    return {true, x, y, z, cx, cy, cz};
}

InteractionResult VoxelInteraction::createMatterAt(const ChunkedGrid<float>& grid, float ox, float oy, float oz,
                                                   float dx, float dy, float dz, float density,
                                                   const Vector4<float, Space::World>& essenceColor,
                                                   float maxDistance) {

    auto hit = castRay(grid, ox, oy, oz, dx, dy, dz, maxDistance);
    if (!hit.has_value())
        return {false, 0, 0, 0, 0, 0, 0};
    return createMatter(*hit, density, essenceColor);
}

InteractionResult VoxelInteraction::destroyMatterAt(const ChunkedGrid<float>& grid, float ox, float oy, float oz,
                                                    float dx, float dy, float dz, float maxDistance) {

    auto hit = castRay(grid, ox, oy, oz, dx, dy, dz, maxDistance);
    if (!hit.has_value())
        return {false, 0, 0, 0, 0, 0, 0};
    return destroyMatter(*hit);
}

bool VoxelInteraction::wouldOverlap(int vx, int vy, int vz, const AABB& playerBounds) {
    AABB voxelBounds(Vec3f(static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)),
                     Vec3f(static_cast<float>(vx + 1), static_cast<float>(vy + 1), static_cast<float>(vz + 1)));
    return voxelBounds.intersects(playerBounds);
}

} // namespace fabric
