#include "fabric/core/VoxelRaycast.hh"
#include "fabric/utils/Profiler.hh"

namespace fabric {

namespace {

struct DDAState {
    int vx, vy, vz;
    int stepX, stepY, stepZ;
    float tMaxX, tMaxY, tMaxZ;
    float tDeltaX, tDeltaY, tDeltaZ;
};

DDAState initDDA(float ox, float oy, float oz, float dx, float dy, float dz) {
    DDAState s{};

    s.vx = static_cast<int>(std::floor(ox));
    s.vy = static_cast<int>(std::floor(oy));
    s.vz = static_cast<int>(std::floor(oz));

    constexpr float kInf = std::numeric_limits<float>::infinity();

    s.stepX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    s.stepY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
    s.stepZ = (dz > 0) ? 1 : (dz < 0) ? -1 : 0;

    if (dx != 0.0f) {
        float boundaryX = (dx > 0) ? static_cast<float>(s.vx + 1) : static_cast<float>(s.vx);
        s.tMaxX = (boundaryX - ox) / dx;
        s.tDeltaX = static_cast<float>(s.stepX) / dx;
    } else {
        s.tMaxX = kInf;
        s.tDeltaX = kInf;
    }

    if (dy != 0.0f) {
        float boundaryY = (dy > 0) ? static_cast<float>(s.vy + 1) : static_cast<float>(s.vy);
        s.tMaxY = (boundaryY - oy) / dy;
        s.tDeltaY = static_cast<float>(s.stepY) / dy;
    } else {
        s.tMaxY = kInf;
        s.tDeltaY = kInf;
    }

    if (dz != 0.0f) {
        float boundaryZ = (dz > 0) ? static_cast<float>(s.vz + 1) : static_cast<float>(s.vz);
        s.tMaxZ = (boundaryZ - oz) / dz;
        s.tDeltaZ = static_cast<float>(s.stepZ) / dz;
    } else {
        s.tMaxZ = kInf;
        s.tDeltaZ = kInf;
    }

    return s;
}

} // namespace

std::optional<VoxelHit> castRay(const ChunkedGrid<float>& grid, float ox, float oy, float oz, float dx, float dy,
                                float dz, float maxDistance, float threshold) {
    FABRIC_ZONE_SCOPED_N("castRay");

    auto s = initDDA(ox, oy, oz, dx, dy, dz);

    // Check starting voxel
    if (grid.get(s.vx, s.vy, s.vz) > threshold) {
        return VoxelHit{s.vx, s.vy, s.vz, 0, 0, 0, 0.0f};
    }

    while (true) {
        int normalX = 0, normalY = 0, normalZ = 0;
        float t;

        if (s.tMaxX < s.tMaxY) {
            if (s.tMaxX < s.tMaxZ) {
                t = s.tMaxX;
                s.vx += s.stepX;
                s.tMaxX += s.tDeltaX;
                normalX = -s.stepX;
            } else {
                t = s.tMaxZ;
                s.vz += s.stepZ;
                s.tMaxZ += s.tDeltaZ;
                normalZ = -s.stepZ;
            }
        } else {
            if (s.tMaxY < s.tMaxZ) {
                t = s.tMaxY;
                s.vy += s.stepY;
                s.tMaxY += s.tDeltaY;
                normalY = -s.stepY;
            } else {
                t = s.tMaxZ;
                s.vz += s.stepZ;
                s.tMaxZ += s.tDeltaZ;
                normalZ = -s.stepZ;
            }
        }

        if (t > maxDistance)
            break;

        if (grid.get(s.vx, s.vy, s.vz) > threshold) {
            return VoxelHit{s.vx, s.vy, s.vz, normalX, normalY, normalZ, t};
        }
    }

    return std::nullopt;
}

std::vector<VoxelHit> castRayAll(const ChunkedGrid<float>& grid, float ox, float oy, float oz, float dx, float dy,
                                 float dz, float maxDistance, float threshold) {
    FABRIC_ZONE_SCOPED_N("castRayAll");

    std::vector<VoxelHit> hits;
    auto s = initDDA(ox, oy, oz, dx, dy, dz);

    if (grid.get(s.vx, s.vy, s.vz) > threshold) {
        hits.push_back({s.vx, s.vy, s.vz, 0, 0, 0, 0.0f});
    }

    while (true) {
        int normalX = 0, normalY = 0, normalZ = 0;
        float t;

        if (s.tMaxX < s.tMaxY) {
            if (s.tMaxX < s.tMaxZ) {
                t = s.tMaxX;
                s.vx += s.stepX;
                s.tMaxX += s.tDeltaX;
                normalX = -s.stepX;
            } else {
                t = s.tMaxZ;
                s.vz += s.stepZ;
                s.tMaxZ += s.tDeltaZ;
                normalZ = -s.stepZ;
            }
        } else {
            if (s.tMaxY < s.tMaxZ) {
                t = s.tMaxY;
                s.vy += s.stepY;
                s.tMaxY += s.tDeltaY;
                normalY = -s.stepY;
            } else {
                t = s.tMaxZ;
                s.vz += s.stepZ;
                s.tMaxZ += s.tDeltaZ;
                normalZ = -s.stepZ;
            }
        }

        if (t > maxDistance)
            break;

        if (grid.get(s.vx, s.vy, s.vz) > threshold) {
            hits.push_back({s.vx, s.vy, s.vz, normalX, normalY, normalZ, t});
        }
    }

    return hits;
}

} // namespace fabric
