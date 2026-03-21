#include "recurse/simulation/EssenceAssigner.hh"
#include "fabric/utils/Profiler.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/VoxelConstants.hh"
#include "recurse/world/EssencePalette.hh"

#include <cstring>

namespace recurse::simulation {

namespace {

inline float spatialHash(int x, int y, int z, int component, int seed) {
    auto u = [](int v) {
        return static_cast<uint32_t>(v);
    };
    uint32_t h = u(x) * 374761393u + u(y) * 668265263u + u(z) * 1274126177u + u(component) * 1103515245u + u(seed);
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFF) / 32767.5f - 1.0f;
}

constexpr float K_NOISE_AMPLITUDE = 0.05f;
constexpr int K_MAX_MATERIALS = 32;

inline float clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

constexpr int ipow(int base, int exp) {
    int r = 1;
    for (int i = 0; i < exp; ++i)
        r *= base;
    return r;
}

// Choose grid steps K per noise component so matCount * K^4 fits in a uint8_t palette.
inline int stepsForMaterialCount(int matCount) {
    if (matCount <= 1)
        return 4; // 1 * 256 = 256
    if (matCount <= 3)
        return 3; // 3 * 81  = 243
    if (matCount <= 16)
        return 2; // 16 * 16 = 256
    return 1;
}

} // namespace

void assignEssence(VoxelCell* buffer, int cx, int cy, int cz, const MaterialRegistry& materials,
                   recurse::EssencePalette& palette, int seed) {
    FABRIC_ZONE_SCOPED_N("assign_essence");

    char coordBuf[48];
    (void)std::snprintf(coordBuf, sizeof(coordBuf), "chunk(%d,%d,%d)", cx, cy, cz);
    FABRIC_ZONE_TEXT(coordBuf, std::strlen(coordBuf));

    // Collect unique non-AIR materials in this chunk.
    uint16_t uniqueMats[K_MAX_MATERIALS];
    int matCount = 0;

    for (int i = 0; i < K_CHUNK_VOLUME; ++i) {
        if (isEmpty(buffer[i]))
            continue;
        auto mid = cellMaterialId(buffer[i]);
        bool found = false;
        for (int m = 0; m < matCount; ++m) {
            if (uniqueMats[m] == mid) {
                found = true;
                break;
            }
        }
        if (!found && matCount < K_MAX_MATERIALS)
            uniqueMats[matCount++] = mid;
    }

    if (matCount == 0)
        return;

    FABRIC_ZONE_VALUE(matCount);

    // Pre-build palette with K^4 noise samples per material, then use a LUT
    // for O(1) per-voxel assignment. matCount * K^4 <= 256 by construction.
    int K = stepsForMaterialCount(matCount);
    int entriesPerMat = ipow(K, 4);
    float step = (2.0f * K_NOISE_AMPLITUDE) / static_cast<float>(K);

    uint8_t paletteLUT[K_MAX_MATERIALS * 256];

    for (int m = 0; m < matCount; ++m) {
        const auto& def = materials.get(uniqueMats[m]);
        for (int idx4 = 0; idx4 < entriesPerMat; ++idx4) {
            int qx = idx4 % K;
            int qy = (idx4 / K) % K;
            int qz = (idx4 / (K * K)) % K;
            int qw = idx4 / (K * K * K);

            float nx = -K_NOISE_AMPLITUDE + (static_cast<float>(qx) + 0.5f) * step;
            float ny = -K_NOISE_AMPLITUDE + (static_cast<float>(qy) + 0.5f) * step;
            float nz = -K_NOISE_AMPLITUDE + (static_cast<float>(qz) + 0.5f) * step;
            float nw = -K_NOISE_AMPLITUDE + (static_cast<float>(qw) + 0.5f) * step;

            fabric::Vector4<float, fabric::Space::World> essence(
                clamp01(def.baseEssence[0] + nx), clamp01(def.baseEssence[1] + ny), clamp01(def.baseEssence[2] + nz),
                clamp01(def.baseEssence[3] + nw));

            uint16_t palIdx = palette.addEntry(essence);
            paletteLUT[m * entriesPerMat + idx4] = static_cast<uint8_t>(palIdx);
        }
    }

    // Per-voxel assignment: quantize noise to grid cell, index into LUT.
    auto quantStep = [&](float noise) -> int {
        int q = static_cast<int>((noise + K_NOISE_AMPLITUDE) / step);
        return (q < 0) ? 0 : (q >= K ? K - 1 : q);
    };

    int baseX = cx * K_CHUNK_SIZE;
    int baseY = cy * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;

    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
                auto& cell = buffer[idx];

                if (isEmpty(cell))
                    continue;

                int matIdx = 0;
                for (int m = 0; m < matCount; ++m) {
                    if (uniqueMats[m] == cellMaterialId(cell)) {
                        matIdx = m;
                        break;
                    }
                }

                int wx = baseX + lx;
                int wy = baseY + ly;
                int wz = baseZ + lz;

                int qx = quantStep(spatialHash(wx, wy, wz, 0, seed) * K_NOISE_AMPLITUDE);
                int qy = quantStep(spatialHash(wx, wy, wz, 1, seed) * K_NOISE_AMPLITUDE);
                int qz = quantStep(spatialHash(wx, wy, wz, 2, seed) * K_NOISE_AMPLITUDE);
                int qw = quantStep(spatialHash(wx, wy, wz, 3, seed) * K_NOISE_AMPLITUDE);

                int noiseIdx = qx + qy * K + qz * K * K + qw * K * K * K;
                cell.spare = paletteLUT[matIdx * entriesPerMat + noiseIdx];
            }
        }
    }
}

} // namespace recurse::simulation
