#include "recurse/simulation/EssenceAssigner.hh"
#include "fabric/utils/Profiler.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/world/EssencePalette.hh"

namespace recurse::simulation {

using fabric::K_CHUNK_SIZE;

namespace {

/// Simple spatial hash returning a float in [-1, 1].
/// Based on a scrambled integer hash (no heap allocation, no FastNoise).
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

inline float clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

} // namespace

void assignEssence(VoxelCell* buffer, int cx, int cy, int cz, const MaterialRegistry& materials,
                   recurse::EssencePalette& palette, int seed) {
    FABRIC_ZONE_SCOPED_N("assign_essence");

    int baseX = cx * K_CHUNK_SIZE;
    int baseY = cy * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;

    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
                auto& cell = buffer[idx];

                if (cell.materialId == material_ids::AIR)
                    continue;

                const auto& def = materials.get(cell.materialId);
                int wx = baseX + lx;
                int wy = baseY + ly;
                int wz = baseZ + lz;

                fabric::Vector4<float, fabric::Space::World> essence(
                    clamp01(def.baseEssence[0] + spatialHash(wx, wy, wz, 0, seed) * K_NOISE_AMPLITUDE),
                    clamp01(def.baseEssence[1] + spatialHash(wx, wy, wz, 1, seed) * K_NOISE_AMPLITUDE),
                    clamp01(def.baseEssence[2] + spatialHash(wx, wy, wz, 2, seed) * K_NOISE_AMPLITUDE),
                    clamp01(def.baseEssence[3] + spatialHash(wx, wy, wz, 3, seed) * K_NOISE_AMPLITUDE));

                cell.essenceIdx = palette.quantize8(essence);
            }
        }
    }
}

} // namespace recurse::simulation
