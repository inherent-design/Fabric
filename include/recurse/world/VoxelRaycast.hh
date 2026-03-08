#pragma once

#include "recurse/world/ChunkedGrid.hh"

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace fabric::simulation {
class SimulationGrid;
} // namespace fabric::simulation

namespace recurse {

struct VoxelHit {
    int x, y, z;
    int nx, ny, nz;
    float t;
};

std::optional<VoxelHit> castRay(const ChunkedGrid<float>& grid, float ox, float oy, float oz, float dx, float dy,
                                float dz, float maxDistance = 256.0f, float threshold = 0.5f);

std::vector<VoxelHit> castRayAll(const ChunkedGrid<float>& grid, float ox, float oy, float oz, float dx, float dy,
                                 float dz, float maxDistance = 256.0f, float threshold = 0.5f);

std::optional<VoxelHit> castRay(const fabric::simulation::SimulationGrid& grid, float ox, float oy, float oz, float dx,
                                float dy, float dz, float maxDistance = 256.0f);

std::vector<VoxelHit> castRayAll(const fabric::simulation::SimulationGrid& grid, float ox, float oy, float oz, float dx,
                                 float dy, float dz, float maxDistance = 256.0f);

} // namespace recurse
