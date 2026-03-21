#include "recurse/world/MinecraftNoiseGenerator.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace recurse {

using simulation::MaterialId;
using simulation::Phase;
namespace material_ids = simulation::material_ids;
using simulation::VoxelCell;

namespace {

/// Migration helper: construct a VoxelCell from a MaterialId using known
/// material properties. Avoids requiring a MaterialRegistry in generators.
VoxelCell cellForMaterial(MaterialId id) {
    using simulation::makeCell;
    switch (id) {
        case material_ids::AIR:
            return VoxelCell{};
        case material_ids::STONE:
            return makeCell(1, Phase::Solid, 200);
        case material_ids::DIRT:
            return makeCell(2, Phase::Solid, 150);
        case material_ids::SAND:
            return makeCell(3, Phase::Powder, 130);
        case material_ids::WATER:
            return makeCell(4, Phase::Liquid, 100);
        case material_ids::GRAVEL:
            return makeCell(5, Phase::Powder, 170);
        default:
            return makeCell(static_cast<uint8_t>(id), Phase::Solid, 128);
    }
}

constexpr float K_FILLED_DENSITY_THRESHOLD = 0.0f;
constexpr uint32_t K_TERRAIN_RULE_VERSION = 3;

int floorChunkCoord(int worldCoord) {
    int chunk = worldCoord / simulation::K_CHUNK_SIZE;
    if (worldCoord < 0 && (worldCoord % simulation::K_CHUNK_SIZE) != 0)
        --chunk;
    return chunk;
}

float chunkAlignedSampleCoord(int worldCoord, float freq) {
    int chunk = floorChunkCoord(worldCoord);
    int base = chunk * simulation::K_CHUNK_SIZE;
    int local = worldCoord - base;
    return std::fma(static_cast<float>(local), freq, static_cast<float>(base) * freq);
}

float sampleChunkAlignedNoise2D(const FastNoise::SmartNode<FastNoise::Simplex>& node, int wx, int wz, float freq,
                                int seed) {
    return node->GenSingle2D(chunkAlignedSampleCoord(wx, freq), chunkAlignedSampleCoord(wz, freq), seed);
}

float saturate(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float remapUnit(float value) {
    return saturate(value * 0.5f + 0.5f);
}

} // namespace

MinecraftNoiseGenerator::MinecraftNoiseGenerator(const NoiseGenConfig& config)
    : config_(config),
      continentalNode_(FastNoise::New<FastNoise::Simplex>()),
      erosionNode_(FastNoise::New<FastNoise::Simplex>()),
      peaksNode_(FastNoise::New<FastNoise::Simplex>()),
      temperatureNode_(FastNoise::New<FastNoise::Simplex>()),
      humidityNode_(FastNoise::New<FastNoise::Simplex>()),
      detailNode_(FastNoise::New<FastNoise::Simplex>()) {}

void MinecraftNoiseGenerator::batchNoise2D(const FastNoise::SmartNode<FastNoise::Simplex>& node, float freq,
                                           float baseX, float baseZ, float* out, int seed) const {
    node->GenUniformGrid2D(out, baseX * freq, baseZ * freq, K_SIZE, K_SIZE, freq, freq, seed);
}

float MinecraftNoiseGenerator::detailFreq() const {
    return std::max(config_.peaksFreq * 2.75f, config_.erosionFreq * 3.5f);
}

MinecraftNoiseGenerator::ColumnSample MinecraftNoiseGenerator::buildColumnSample(float continental, float erosion,
                                                                                 float peaks, float temperature,
                                                                                 float humidity, float detail) const {
    const float warmth = remapUnit(temperature);
    const float wetness = remapUnit(humidity);
    const float dryness = 1.0f - wetness;
    const float erosionSoftness = remapUnit(erosion);
    const float ridgeSignal = std::max(peaks, 0.0f);
    const float basinSignal = std::max(-continental, 0.0f);

    const float ruggedness =
        saturate(0.18f + ridgeSignal * 0.55f + (1.0f - erosionSoftness) * 0.22f + std::max(continental, 0.0f) * 0.10f);

    const float macroShape = continental * 0.58f + peaks * (0.12f + 0.20f * (1.0f - erosionSoftness)) - erosion * 0.15f;
    const float ridgeBoost = ridgeSignal * (0.18f + 0.18f * ruggedness);
    const float basinCut = basinSignal * (0.16f + 0.12f * wetness);
    const float detailAmplitude = config_.terrainHeight * (0.05f + 0.05f * ruggedness + 0.03f * dryness);
    const float surfaceHeight =
        config_.seaLevel + (macroShape + ridgeBoost - basinCut) * config_.terrainHeight + detail * detailAmplitude;

    const float shorelineBand = 4.0f + 4.0f * wetness + 3.0f * (1.0f - ruggedness);
    const float coastalness = saturate(1.0f - std::abs(surfaceHeight - config_.seaLevel) / shorelineBand);
    const float sediment = saturate(0.18f + 0.42f * coastalness + 0.18f * dryness + 0.12f * basinSignal -
                                    0.30f * ruggedness + 0.10f * warmth);

    float surfaceDepth = 1.0f + 1.6f * wetness + 0.8f * coastalness - 0.85f * ruggedness;
    surfaceDepth = std::clamp(surfaceDepth, 1.0f, 3.25f);

    float sedimentDepth = surfaceDepth + 0.9f + 2.0f * coastalness + 0.8f * sediment - 0.45f * ruggedness;
    sedimentDepth = std::clamp(sedimentDepth, surfaceDepth + 0.5f, 5.5f);

    return ColumnSample{
        .surfaceHeight = surfaceHeight,
        .warmth = warmth,
        .wetness = wetness,
        .ruggedness = ruggedness,
        .coastalness = coastalness,
        .sediment = sediment,
        .surfaceDepth = surfaceDepth,
        .sedimentDepth = sedimentDepth,
    };
}

std::string MinecraftNoiseGenerator::worldgenFingerprintSource() const {
    return std::string{"terrain-baseline:minecraft-noise-heightfield|ruleVersion="} +
           std::to_string(K_TERRAIN_RULE_VERSION) + "|seed=" + std::to_string(config_.seed) +
           "|seaLevel=" + std::to_string(config_.seaLevel) + "|terrainHeight=" + std::to_string(config_.terrainHeight) +
           "|continentalFreq=" + std::to_string(config_.continentalFreq) +
           "|erosionFreq=" + std::to_string(config_.erosionFreq) + "|peaksFreq=" + std::to_string(config_.peaksFreq) +
           "|temperatureFreq=" + std::to_string(config_.temperatureFreq) +
           "|humidityFreq=" + std::to_string(config_.humidityFreq);
}

MaterialId MinecraftNoiseGenerator::selectSurfaceMaterial(const ColumnSample& column) const {
    if (column.coastalness > 0.45f) {
        return (column.sediment > 0.58f) ? material_ids::SAND : material_ids::GRAVEL;
    }

    if (column.warmth > 0.66f && column.wetness < 0.36f && column.sediment > 0.48f) {
        return material_ids::SAND;
    }

    if (column.ruggedness > 0.72f && column.sediment < 0.62f) {
        return material_ids::GRAVEL;
    }

    return material_ids::DIRT;
}

MinecraftNoiseGenerator::ColumnSample MinecraftNoiseGenerator::sampleColumn(int wx, int wz) const {
    const int continentalSeed = config_.seed;
    const int erosionSeed = config_.seed + 1;
    const int peaksSeed = config_.seed + 2;
    const int temperatureSeed = config_.seed + 3;
    const int humiditySeed = config_.seed + 4;
    const int detailSeed = config_.seed + 5;
    const float detailFrequency = detailFreq();

    float c = sampleChunkAlignedNoise2D(continentalNode_, wx, wz, config_.continentalFreq, continentalSeed);
    float e = sampleChunkAlignedNoise2D(erosionNode_, wx, wz, config_.erosionFreq, erosionSeed);
    float p = sampleChunkAlignedNoise2D(peaksNode_, wx, wz, config_.peaksFreq, peaksSeed);
    float temp = sampleChunkAlignedNoise2D(temperatureNode_, wx, wz, config_.temperatureFreq, temperatureSeed);
    float humid = sampleChunkAlignedNoise2D(humidityNode_, wx, wz, config_.humidityFreq, humiditySeed);
    float detail = sampleChunkAlignedNoise2D(detailNode_, wx, wz, detailFrequency, detailSeed);

    return buildColumnSample(c, e, p, temp, humid, detail);
}

uint16_t MinecraftNoiseGenerator::classifyMaterial(const ColumnSample& column, int wy) const {
    float density = column.surfaceHeight - static_cast<float>(wy);
    if (density > column.sedimentDepth)
        return material_ids::STONE;
    if (density > column.surfaceDepth)
        return selectSubsurfaceMaterial(column);
    if (density > K_FILLED_DENSITY_THRESHOLD)
        return selectSurfaceMaterial(column);
    if (static_cast<float>(wy) < config_.seaLevel)
        return material_ids::WATER;
    return material_ids::AIR;
}

simulation::MaterialId MinecraftNoiseGenerator::selectSubsurfaceMaterial(const ColumnSample& column) const {
    if (column.coastalness > 0.35f) {
        return (column.sediment > 0.52f) ? material_ids::SAND : material_ids::GRAVEL;
    }

    if (column.warmth > 0.60f && column.wetness < 0.38f && column.sediment > 0.50f) {
        return material_ids::SAND;
    }

    if (column.ruggedness > 0.62f) {
        return material_ids::GRAVEL;
    }

    return material_ids::DIRT;
}

int MinecraftNoiseGenerator::conservativeVisibleTopY(float surfaceHeight) const {
    return static_cast<int>(std::ceil(std::max(surfaceHeight, config_.seaLevel)));
}

void MinecraftNoiseGenerator::generate(simulation::SimulationGrid& grid, int cx, int cy, int cz) {
    grid.materializeChunk(cx, cy, cz);
    auto* buf = grid.writeBuffer(cx, cy, cz);
    if (!buf)
        return;
    generateToBuffer(buf->data(), cx, cy, cz);
}

void MinecraftNoiseGenerator::generateToBuffer(VoxelCell* buffer, int cx, int cy, int cz) {
    int baseX = cx * K_SIZE;
    int baseY = cy * K_SIZE;
    int baseZ = cz * K_SIZE;
    const int continentalSeed = config_.seed;
    const int erosionSeed = config_.seed + 1;
    const int peaksSeed = config_.seed + 2;
    const int temperatureSeed = config_.seed + 3;
    const int humiditySeed = config_.seed + 4;
    const int detailSeed = config_.seed + 5;
    const float detailFrequency = detailFreq();

    // D-33: Keep low-frequency terrain channels batched and heap-backed for speed,
    // while sampling high-sensitivity detail per-column to preserve far-origin
    // alignment with sampleMaterial().
    std::vector<float> continental(K_SIZE * K_SIZE);
    std::vector<float> erosion(K_SIZE * K_SIZE);
    std::vector<float> peaks(K_SIZE * K_SIZE);
    std::vector<float> temperature(K_SIZE * K_SIZE);
    std::vector<float> humidity(K_SIZE * K_SIZE);

    batchNoise2D(continentalNode_, config_.continentalFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 continental.data(), continentalSeed);
    batchNoise2D(erosionNode_, config_.erosionFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 erosion.data(), erosionSeed);
    batchNoise2D(peaksNode_, config_.peaksFreq, static_cast<float>(baseX), static_cast<float>(baseZ), peaks.data(),
                 peaksSeed);
    batchNoise2D(temperatureNode_, config_.temperatureFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 temperature.data(), temperatureSeed);
    batchNoise2D(humidityNode_, config_.humidityFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 humidity.data(), humiditySeed);

    for (int lz = 0; lz < K_SIZE; ++lz) {
        for (int lx = 0; lx < K_SIZE; ++lx) {
            const int idx2d = lz * K_SIZE + lx;
            const int wx = baseX + lx;
            const int wz = baseZ + lz;
            const float detailSample = sampleChunkAlignedNoise2D(detailNode_, wx, wz, detailFrequency, detailSeed);
            ColumnSample column = buildColumnSample(continental[idx2d], erosion[idx2d], peaks[idx2d],
                                                    temperature[idx2d], humidity[idx2d], detailSample);

            for (int ly = 0; ly < K_SIZE; ++ly) {
                int wy = baseY + ly;
                const uint16_t materialId = classifyMaterial(column, wy);
                if (materialId == material_ids::AIR) {
                    continue;
                }

                VoxelCell cell = cellForMaterial(materialId);
                int idx = lx + ly * K_SIZE + lz * K_SIZE * K_SIZE;
                buffer[idx] = cell;
            }
        }
    }
}

int MinecraftNoiseGenerator::maxSurfaceHeight(int cx, int cz) const {
    const int baseX = cx * K_SIZE;
    const int baseZ = cz * K_SIZE;
    const int continentalSeed = config_.seed;
    const int erosionSeed = config_.seed + 1;
    const int peaksSeed = config_.seed + 2;
    const int detailSeed = config_.seed + 5;
    const float detailFrequency = detailFreq();

    std::array<float, K_SIZE * K_SIZE> continental{};
    std::array<float, K_SIZE * K_SIZE> erosion{};
    std::array<float, K_SIZE * K_SIZE> peaks{};
    batchNoise2D(continentalNode_, config_.continentalFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 continental.data(), continentalSeed);
    batchNoise2D(erosionNode_, config_.erosionFreq, static_cast<float>(baseX), static_cast<float>(baseZ),
                 erosion.data(), erosionSeed);
    batchNoise2D(peaksNode_, config_.peaksFreq, static_cast<float>(baseX), static_cast<float>(baseZ), peaks.data(),
                 peaksSeed);

    float maxSurfaceHeight = -std::numeric_limits<float>::infinity();
    for (int lz = 0; lz < K_SIZE; ++lz) {
        for (int lx = 0; lx < K_SIZE; ++lx) {
            const int idx = lz * K_SIZE + lx;
            const int wx = baseX + lx;
            const int wz = baseZ + lz;
            const float detailSample = sampleChunkAlignedNoise2D(detailNode_, wx, wz, detailFrequency, detailSeed);
            maxSurfaceHeight = std::max(
                maxSurfaceHeight,
                buildColumnSample(continental[idx], erosion[idx], peaks[idx], 0.0f, 0.0f, detailSample).surfaceHeight);
        }
    }

    return conservativeVisibleTopY(maxSurfaceHeight);
}

uint16_t MinecraftNoiseGenerator::sampleMaterial(int wx, int wy, int wz) const {
    return classifyMaterial(sampleColumn(wx, wz), wy);
}

} // namespace recurse
