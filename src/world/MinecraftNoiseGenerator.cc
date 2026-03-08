#include "fabric/world/MinecraftNoiseGenerator.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <cmath>

namespace fabric::world {

using recurse::simulation::MaterialId;
namespace material_ids = recurse::simulation::material_ids;
using recurse::simulation::VoxelCell;

MinecraftNoiseGenerator::MinecraftNoiseGenerator(const NoiseGenConfig& config)
    : config_(config),
      continentalNode_(FastNoise::New<FastNoise::Simplex>()),
      erosionNode_(FastNoise::New<FastNoise::Simplex>()),
      peaksNode_(FastNoise::New<FastNoise::Simplex>()),
      temperatureNode_(FastNoise::New<FastNoise::Simplex>()),
      humidityNode_(FastNoise::New<FastNoise::Simplex>()) {}

void MinecraftNoiseGenerator::batchNoise2D(const FastNoise::SmartNode<FastNoise::Simplex>& node, float freq,
                                           float baseX, float baseZ, std::array<float, K_SIZE * K_SIZE>& out,
                                           int seed) const {
    // GenUniformGrid2D layout: out[z * xCount + x] (row-major, X inner loop)
    // We want out[lz * K_SIZE + lx] = noise at (baseX + lx, baseZ + lz) * freq
    // GenUniformGrid2D(out, xOffset, yOffset, xCount, yCount, xStep, yStep, seed)
    // Maps: x-axis = world X, y-axis = world Z
    node->GenUniformGrid2D(out.data(), baseX * freq, baseZ * freq, K_SIZE, K_SIZE, freq, freq, seed);
}

float MinecraftNoiseGenerator::computeBaseHeight(float continental, float erosion, float peaks) const {
    return config_.seaLevel + (continental * 0.6f + peaks * 0.3f - erosion * 0.2f) * config_.terrainHeight;
}

MaterialId MinecraftNoiseGenerator::selectSurfaceMaterial(float temp, float humid, float wy) const {
    // TODO(human): Implement biome-aware surface material selection.
    // Inputs: temp [-1,1], humid [-1,1], wy (world Y coordinate).
    // Available materials: Sand, Dirt, Gravel (from MaterialIds).
    // Should return the appropriate MaterialId based on biome conditions.
    // Current fallback: Sand near sea level, Dirt elsewhere.
    if (std::abs(wy - config_.seaLevel) <= 3.0f) {
        return material_ids::SAND;
    }
    return material_ids::DIRT;
}

void MinecraftNoiseGenerator::generate(recurse::simulation::SimulationGrid& grid, recurse::simulation::ChunkPos pos) {
    grid.materializeChunk(pos.x, pos.y, pos.z);

    int baseX = pos.x * K_SIZE;
    int baseY = pos.y * K_SIZE;
    int baseZ = pos.z * K_SIZE;

    // Batch 2D noise for all 5 layers (32x32 each)
    std::array<float, K_SIZE * K_SIZE> continental{};
    std::array<float, K_SIZE * K_SIZE> erosion{};
    std::array<float, K_SIZE * K_SIZE> peaks{};
    std::array<float, K_SIZE * K_SIZE> temperature{};
    std::array<float, K_SIZE * K_SIZE> humidity{};

    // Each layer uses a different seed offset to decorrelate noise functions.
    // Without this, all layers sample the same Simplex function and produce
    // correlated outputs, collapsing terrain variation.
    batchNoise2D(continentalNode_, config_.continentalFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 continental, config_.seed);
    batchNoise2D(erosionNode_, config_.erosionFreq, static_cast<float>(baseX), static_cast<float>(baseZ), erosion,
                 config_.seed + 1);
    batchNoise2D(peaksNode_, config_.peaksFreq, static_cast<float>(baseX), static_cast<float>(baseZ), peaks,
                 config_.seed + 2);
    batchNoise2D(temperatureNode_, config_.temperatureFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 temperature, config_.seed + 3);
    batchNoise2D(humidityNode_, config_.humidityFreq, static_cast<float>(baseX), static_cast<float>(baseZ), humidity,
                 config_.seed + 4);

    for (int lz = 0; lz < K_SIZE; ++lz) {
        for (int lx = 0; lx < K_SIZE; ++lx) {
            int idx2d = lz * K_SIZE + lx;
            int wx = baseX + lx;
            int wz = baseZ + lz;

            float c = continental[idx2d];
            float e = erosion[idx2d];
            float p = peaks[idx2d];
            float temp = temperature[idx2d];
            float humid = humidity[idx2d];

            float bh = computeBaseHeight(c, e, p);

            for (int ly = 0; ly < K_SIZE; ++ly) {
                int wy = baseY + ly;
                float density = bh - static_cast<float>(wy);

                VoxelCell cell;
                if (density > 3.0f) {
                    cell.materialId = material_ids::STONE;
                } else if (density > 0.0f) {
                    cell.materialId = selectSurfaceMaterial(temp, humid, static_cast<float>(wy));
                } else if (static_cast<float>(wy) < config_.seaLevel) {
                    cell.materialId = material_ids::WATER;
                } else {
                    continue; // Air -- skip write (default)
                }

                grid.writeCell(wx, wy, wz, cell);
            }
        }
    }
}

} // namespace fabric::world
