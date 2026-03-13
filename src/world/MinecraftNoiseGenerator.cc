#include "fabric/world/MinecraftNoiseGenerator.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <cmath>
#include <vector>

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
                                           float baseX, float baseZ, float* out, int seed) const {
    node->GenUniformGrid2D(out, baseX * freq, baseZ * freq, K_SIZE, K_SIZE, freq, freq, seed);
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

void MinecraftNoiseGenerator::generate(recurse::simulation::SimulationGrid& grid, recurse::simulation::ChunkCoord pos) {
    grid.materializeChunk(pos.x, pos.y, pos.z);
    auto* buf = grid.writeBuffer(pos.x, pos.y, pos.z);
    if (!buf)
        return;
    generateToBuffer(buf->data(), pos.x, pos.y, pos.z);
}

void MinecraftNoiseGenerator::generateToBuffer(VoxelCell* buffer, int cx, int cy, int cz) const {
    int baseX = cx * K_SIZE;
    int baseY = cy * K_SIZE;
    int baseZ = cz * K_SIZE;

    // D-33: Heap-allocated noise arrays. 5x4KB on stack risks overflow on
    // enkiTS 64KB fiber stacks during parallel world generation (C-P1).
    std::vector<float> continental(K_SIZE * K_SIZE);
    std::vector<float> erosion(K_SIZE * K_SIZE);
    std::vector<float> peaks(K_SIZE * K_SIZE);
    std::vector<float> temperature(K_SIZE * K_SIZE);
    std::vector<float> humidity(K_SIZE * K_SIZE);

    batchNoise2D(continentalNode_, config_.continentalFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 continental.data(), config_.seed);
    batchNoise2D(erosionNode_, config_.erosionFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 erosion.data(), config_.seed + 1);
    batchNoise2D(peaksNode_, config_.peaksFreq, static_cast<float>(baseX), static_cast<float>(baseZ), peaks.data(),
                 config_.seed + 2);
    batchNoise2D(temperatureNode_, config_.temperatureFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 temperature.data(), config_.seed + 3);
    batchNoise2D(humidityNode_, config_.humidityFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 humidity.data(), config_.seed + 4);

    for (int lz = 0; lz < K_SIZE; ++lz) {
        for (int lx = 0; lx < K_SIZE; ++lx) {
            int idx2d = lz * K_SIZE + lx;

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
                    continue;
                }

                int idx = lx + ly * K_SIZE + lz * K_SIZE * K_SIZE;
                buffer[idx] = cell;
            }
        }
    }
}

int MinecraftNoiseGenerator::maxSurfaceHeight(int cx, int cz) const {
    float centerX = static_cast<float>(cx * K_SIZE + K_SIZE / 2);
    float centerZ = static_cast<float>(cz * K_SIZE + K_SIZE / 2);

    float c = continentalNode_->GenSingle2D(centerX * config_.continentalFreq, centerZ * config_.continentalFreq,
                                            config_.seed);
    float e = erosionNode_->GenSingle2D(centerX * config_.erosionFreq, centerZ * config_.erosionFreq, config_.seed + 1);
    float p = peaksNode_->GenSingle2D(centerX * config_.peaksFreq, centerZ * config_.peaksFreq, config_.seed + 2);

    float bh = computeBaseHeight(c, e, p);

    constexpr float K_MARGIN = 16.0f;
    float maxHeight = std::max(bh + K_MARGIN, config_.seaLevel);

    return static_cast<int>(std::ceil(maxHeight));
}

uint16_t MinecraftNoiseGenerator::sampleMaterial(int wx, int wy, int wz) const {
    float fx = static_cast<float>(wx);
    float fz = static_cast<float>(wz);

    float c = continentalNode_->GenSingle2D(fx * config_.continentalFreq, fz * config_.continentalFreq, config_.seed);
    float e = erosionNode_->GenSingle2D(fx * config_.erosionFreq, fz * config_.erosionFreq, config_.seed + 1);
    float p = peaksNode_->GenSingle2D(fx * config_.peaksFreq, fz * config_.peaksFreq, config_.seed + 2);

    float bh = computeBaseHeight(c, e, p);
    float density = bh - static_cast<float>(wy);

    if (density > 3.0f)
        return material_ids::STONE;
    if (density > 0.0f)
        return selectSurfaceMaterial(0.0f, 0.0f, static_cast<float>(wy));
    if (static_cast<float>(wy) < config_.seaLevel)
        return material_ids::WATER;
    return material_ids::AIR;
}

} // namespace fabric::world
