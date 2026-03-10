#pragma once
#include "fabric/world/GeneratorInterface.hh"
#include <array>
#include <FastNoise/FastNoise.h>
#include <vector>

namespace fabric::world {

struct NoiseGenConfig {
    int seed = 42;
    float seaLevel = 48.0f;
    float terrainHeight = 64.0f;
    float continentalFreq = 0.006f; // 2x from 0.003f - smaller features
    float erosionFreq = 0.016f;     // 2x from 0.008f
    float peaksFreq = 0.030f;       // 2x from 0.015f
    float temperatureFreq = 0.004f; // 2x from 0.002f
    float humidityFreq = 0.004f;    // 2x from 0.002f
};

class MinecraftNoiseGenerator : public GeneratorInterface {
  public:
    explicit MinecraftNoiseGenerator(const NoiseGenConfig& config = {});
    void generate(recurse::simulation::SimulationGrid& grid, recurse::simulation::ChunkPos pos) override;
    std::string name() const override { return "MinecraftNoise"; }

    /// Point-query material at a single world coordinate.
    /// Evaluates 3 noise functions (continental, erosion, peaks) via GenSingle2D
    /// and applies the same density/material logic as generate().
    uint16_t sampleMaterial(int wx, int wy, int wz) const;

    /// Conservative upper bound on the maximum Y where visible material exists
    /// in the chunk column at (cx, cz). Evaluates noise at chunk center with
    /// a margin for intra-chunk variation.
    int maxSurfaceHeight(int cx, int cz) const;

    /// Fill a pre-allocated buffer with voxel data for chunk (cx, cy, cz).
    /// Buffer must hold K_CHUNK_VOLUME VoxelCells, zero-initialized (air).
    /// Thread-safe: reads only immutable generator state.
    void generateToBuffer(recurse::simulation::VoxelCell* buffer, int cx, int cy, int cz) const;

  private:
    NoiseGenConfig config_;
    FastNoise::SmartNode<FastNoise::Simplex> continentalNode_;
    FastNoise::SmartNode<FastNoise::Simplex> erosionNode_;
    FastNoise::SmartNode<FastNoise::Simplex> peaksNode_;
    FastNoise::SmartNode<FastNoise::Simplex> temperatureNode_;
    FastNoise::SmartNode<FastNoise::Simplex> humidityNode_;

    static constexpr int K_SIZE = 32;

    void batchNoise2D(const FastNoise::SmartNode<FastNoise::Simplex>& node, float freq, float baseX, float baseZ,
                      float* out, int seed) const;

    float computeBaseHeight(float continental, float erosion, float peaks) const;

    recurse::simulation::MaterialId selectSurfaceMaterial(float temp, float humid, float wy) const;
};

} // namespace fabric::world
