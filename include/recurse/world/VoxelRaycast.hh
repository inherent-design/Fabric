#pragma once

#include "fabric/world/ChunkedGrid.hh"

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace recurse::simulation {
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {

using fabric::ChunkedGrid;

struct VoxelHit {
    int x, y, z;
    int nx, ny, nz;
    float t;
};

std::optional<VoxelHit> castRay(const ChunkedGrid<float, 32>& grid, float ox, float oy, float oz, float dx, float dy,
                                float dz, float maxDistance = 256.0f, float threshold = 0.5f);

std::vector<VoxelHit> castRayAll(const ChunkedGrid<float, 32>& grid, float ox, float oy, float oz, float dx, float dy,
                                 float dz, float maxDistance = 256.0f, float threshold = 0.5f);

std::optional<VoxelHit> castRay(const recurse::simulation::SimulationGrid& grid, float ox, float oy, float oz, float dx,
                                float dy, float dz, float maxDistance = 256.0f);

std::vector<VoxelHit> castRayAll(const recurse::simulation::SimulationGrid& grid, float ox, float oy, float oz,
                                 float dx, float dy, float dz, float maxDistance = 256.0f);

} // namespace recurse
