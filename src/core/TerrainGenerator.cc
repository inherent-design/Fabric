#include "fabric/core/TerrainGenerator.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>
#include <cmath>
#include <FastNoise/FastNoise.h>
#include <vector>

namespace fabric {

TerrainGenerator::TerrainGenerator(const TerrainConfig& config) : config_(config) {}

const TerrainConfig& TerrainGenerator::config() const {
    return config_;
}

void TerrainGenerator::setConfig(const TerrainConfig& config) {
    config_ = config;
}

void TerrainGenerator::generate(FieldLayer<float>& density, FieldLayer<Vector4<float, Space::World>>& essence,
                                const AABB& region) {
    FABRIC_ZONE_SCOPED;

    // Create noise source based on configured type
    FastNoise::SmartNode<> noiseSource;
    switch (config_.noiseType) {
        case NoiseType::Simplex:
            noiseSource = FastNoise::New<FastNoise::Simplex>();
            break;
        case NoiseType::Perlin:
            noiseSource = FastNoise::New<FastNoise::Perlin>();
            break;
        case NoiseType::OpenSimplex2:
            noiseSource = FastNoise::New<FastNoise::SuperSimplex>();
            break;
        case NoiseType::Value:
            noiseSource = FastNoise::New<FastNoise::Value>();
            break;
    }

    // Set up fractal FBm with the noise source
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(noiseSource);
    fractal->SetOctaveCount(config_.octaves);
    fractal->SetLacunarity(config_.lacunarity);
    fractal->SetGain(config_.gain);

    // Compute integer bounds from AABB (floor min, ceil max)
    int minX = static_cast<int>(std::floor(region.min.x));
    int minY = static_cast<int>(std::floor(region.min.y));
    int minZ = static_cast<int>(std::floor(region.min.z));
    int maxX = static_cast<int>(std::ceil(region.max.x));
    int maxY = static_cast<int>(std::ceil(region.max.y));
    int maxZ = static_cast<int>(std::ceil(region.max.z));

    int sizeX = maxX - minX;
    int sizeY = maxY - minY;
    int sizeZ = maxZ - minZ;

    if (sizeX <= 0 || sizeY <= 0 || sizeZ <= 0) {
        return;
    }

    // Generate noise in a flat buffer using GenUniformGrid3D.
    // FastNoise2 scale = 1/frequency, so we pass step = frequency for coordinate spacing.
    // GenUniformGrid3D: noiseOut, xOffset, yOffset, zOffset, xCount, yCount, zCount,
    //                   xStepSize, yStepSize, zStepSize, seed
    std::vector<float> noiseBuffer(static_cast<size_t>(sizeX) * static_cast<size_t>(sizeY) *
                                   static_cast<size_t>(sizeZ));

    fractal->GenUniformGrid3D(noiseBuffer.data(), static_cast<float>(minX), static_cast<float>(minY),
                              static_cast<float>(minZ), sizeX, sizeY, sizeZ, config_.frequency, config_.frequency,
                              config_.frequency, config_.seed);

    // Write density and essence into field layers.
    // FastNoise2 outputs roughly [-1, 1]; remap to [0, 1].
    size_t idx = 0;
    for (int z = minZ; z < maxZ; ++z) {
        for (int y = minY; y < maxY; ++y) {
            for (int x = minX; x < maxX; ++x) {
                float raw = noiseBuffer[idx++];
                float d = std::clamp(raw * 0.5f + 0.5f, 0.0f, 1.0f);
                density.write(x, y, z, d);

                // Derive essence from position: normalized spatial coordinates
                // plus density as alpha channel
                float nx = static_cast<float>(x - minX) / static_cast<float>(std::max(sizeX - 1, 1));
                float ny = static_cast<float>(y - minY) / static_cast<float>(std::max(sizeY - 1, 1));
                float nz = static_cast<float>(z - minZ) / static_cast<float>(std::max(sizeZ - 1, 1));
                essence.write(x, y, z, Vector4<float, Space::World>(nx, ny, nz, d));
            }
        }
    }
}

} // namespace fabric
