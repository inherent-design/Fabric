#include "recurse/physics/VoxelCollision.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <cmath>

namespace recurse::physics {

AABB getAABB(const Vec3f& pos, float width, float height, float depth) {
    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    Vec3f minCorner(pos.x - hw, pos.y, pos.z - hd);
    Vec3f maxCorner(pos.x + hw, pos.y + height, pos.z + hd);
    return AABB(minCorner, maxCorner);
}

bool isSolid(int vx, int vy, int vz, const ChunkedGrid<float>& grid, float threshold) {
    return grid.get(vx, vy, vz) >= threshold;
}

bool aabbOverlapsSolid(const AABB& box, const ChunkedGrid<float>& grid, float threshold, float epsilon) {
    int minVX = static_cast<int>(std::floor(box.min.x));
    int minVY = static_cast<int>(std::floor(box.min.y));
    int minVZ = static_cast<int>(std::floor(box.min.z));
    int maxVX = static_cast<int>(std::floor(box.max.x - epsilon));
    int maxVY = static_cast<int>(std::floor(box.max.y - epsilon));
    int maxVZ = static_cast<int>(std::floor(box.max.z - epsilon));

    for (int vy = minVY; vy <= maxVY; ++vy)
        for (int vz = minVZ; vz <= maxVZ; ++vz)
            for (int vx = minVX; vx <= maxVX; ++vx)
                if (isSolid(vx, vy, vz, grid, threshold))
                    return true;
    return false;
}

bool isSolid(int vx, int vy, int vz, const recurse::simulation::SimulationGrid& grid) {
    return recurse::simulation::isOccupied(grid.readCell(vx, vy, vz));
}

bool aabbOverlapsSolid(const AABB& box, const recurse::simulation::SimulationGrid& grid, float epsilon) {
    int minVX = static_cast<int>(std::floor(box.min.x));
    int minVY = static_cast<int>(std::floor(box.min.y));
    int minVZ = static_cast<int>(std::floor(box.min.z));
    int maxVX = static_cast<int>(std::floor(box.max.x - epsilon));
    int maxVY = static_cast<int>(std::floor(box.max.y - epsilon));
    int maxVZ = static_cast<int>(std::floor(box.max.z - epsilon));

    for (int vy = minVY; vy <= maxVY; ++vy)
        for (int vz = minVZ; vz <= maxVZ; ++vz)
            for (int vx = minVX; vx <= maxVX; ++vx)
                if (isSolid(vx, vy, vz, grid))
                    return true;
    return false;
}

} // namespace recurse::physics
