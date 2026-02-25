#include "fabric/core/CaveCarver.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>
#include <cmath>
#include <FastNoise/FastNoise.h>
#include <vector>

namespace fabric {

CaveCarver::CaveCarver(CaveConfig config) : config_(config) {}

const CaveConfig& CaveCarver::config() const {
    return config_;
}

void CaveCarver::setConfig(const CaveConfig& config) {
    config_ = config;
}

void CaveCarver::carve(FieldLayer<float>& density, const AABB& region) {
    FABRIC_ZONE_SCOPED;

    // Build cellular noise for worm-like cave systems.
    // The approach: use Cellular distance output to create tube-like cavities.
    // CellularDistance gives low values near Voronoi cell edges, which when
    // inverted creates worm-shaped tunnels along the edges between cells.
    //
    // We pass step=1.0 to GenUniformGrid3D so voxel coordinates map directly
    // to noise space. The generator's SetScale controls cell size in voxels.
    auto cellular = FastNoise::New<FastNoise::CellularDistance>();
    // Scale = 1/frequency gives cell size in voxels.
    // frequency=0.05 -> scale=20 -> one Voronoi cell spans ~20 voxels.
    if (config_.frequency > 0.0f) {
        cellular->SetScale(1.0f / config_.frequency);
    }

    // Add domain warp for more organic, winding tunnels
    auto warp = FastNoise::New<FastNoise::DomainWarpGradient>();
    warp->SetSource(cellular);
    warp->SetWarpAmplitude(config_.worminess * 30.0f);
    // Warp scale controls the spatial frequency of the warp pattern itself.
    // Use 2x the cellular scale for broader warping.
    if (config_.frequency > 0.0f) {
        warp->SetScale(2.0f / config_.frequency);
    }

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

    std::vector<float> noiseBuffer(static_cast<size_t>(sizeX) * static_cast<size_t>(sizeY) *
                                   static_cast<size_t>(sizeZ));

    // Step = 1.0: each voxel index maps to one unit in noise space.
    // The generator's SetScale controls actual feature size.
    warp->GenUniformGrid3D(noiseBuffer.data(), static_cast<float>(minX), static_cast<float>(minY),
                           static_cast<float>(minZ), sizeX, sizeY, sizeZ, 1.0f, 1.0f, 1.0f, config_.seed);

    // Process noise output to carve density.
    // Cellular distance returns values roughly in [0, 1]; invert so that low
    // distance (near cell edges) becomes high cave probability.
    // A threshold check then determines which voxels get carved.
    size_t idx = 0;
    for (int z = minZ; z < maxZ; ++z) {
        for (int y = minY; y < maxY; ++y) {
            for (int x = minX; x < maxX; ++x) {
                float raw = noiseBuffer[idx++];

                // Cellular distance is in ~[0, 1]; invert for worm tunnels.
                // Values near 0 are near cell edges (tunnel centers).
                float caveStrength = 1.0f - std::clamp(std::abs(raw), 0.0f, 1.0f);

                if (caveStrength > config_.threshold) {
                    // Scale carving intensity based on how far above threshold
                    float intensity = (caveStrength - config_.threshold) / (1.0f - config_.threshold);
                    intensity = std::clamp(intensity, 0.0f, 1.0f);

                    // Apply radius-based tapering: full carve at center,
                    // partial at edges
                    float carveAmount = intensity;

                    float current = density.read(x, y, z);
                    float carved = current - carveAmount;
                    density.write(x, y, z, std::max(carved, 0.0f));
                }
            }
        }
    }
}

} // namespace fabric
