#pragma once

#include "recurse/simulation/VoxelConstants.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/WorldGenerator.hh"
#include <array>
#include <FastNoise/FastNoise.h>
#include <vector>

namespace recurse {

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

class MinecraftNoiseGenerator : public WorldGenerator {
  public:
    explicit MinecraftNoiseGenerator(const NoiseGenConfig& config = {});
    void generate(simulation::SimulationGrid& grid, int cx, int cy, int cz) override;
    std::string name() const override { return "MinecraftNoise"; }

    uint16_t sampleMaterial(int wx, int wy, int wz) const override;
    int maxSurfaceHeight(int cx, int cz) const override;
    void generateToBuffer(simulation::VoxelCell* buffer, int cx, int cy, int cz) override;

  private:
    NoiseGenConfig config_;
    FastNoise::SmartNode<FastNoise::Simplex> continentalNode_;
    FastNoise::SmartNode<FastNoise::Simplex> erosionNode_;
    FastNoise::SmartNode<FastNoise::Simplex> peaksNode_;
    FastNoise::SmartNode<FastNoise::Simplex> temperatureNode_;
    FastNoise::SmartNode<FastNoise::Simplex> humidityNode_;

    static constexpr int K_SIZE = simulation::K_CHUNK_SIZE;

    void batchNoise2D(const FastNoise::SmartNode<FastNoise::Simplex>& node, float freq, float baseX, float baseZ,
                      float* out, int seed) const;

    float computeBaseHeight(float continental, float erosion, float peaks) const;

    simulation::MaterialId selectSurfaceMaterial(float temp, float humid, float wy) const;
};

} // namespace recurse
