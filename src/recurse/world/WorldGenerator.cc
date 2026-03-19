#include "recurse/world/WorldGenerator.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <algorithm>
#include <cstring>
#include <string_view>

namespace recurse {

using simulation::K_CHUNK_VOLUME;
using simulation::VoxelCell;

namespace {

uint32_t hashFNV1a(std::string_view text) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : text) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

} // namespace

void WorldGenerator::generateToBuffer(VoxelCell* buffer, int cx, int cy, int cz) {
    // Fallback for generators that do not override: run generate() against a
    // temporary single-chunk grid and copy the result into the caller's buffer.
    simulation::SimulationGrid tmpGrid;
    generate(tmpGrid, cx, cy, cz);

    if (tmpGrid.isChunkMaterialized(cx, cy, cz)) {
        tmpGrid.syncChunkBuffers(cx, cy, cz);
        const auto* buf = tmpGrid.readBuffer(cx, cy, cz);
        if (buf) {
            std::memcpy(buffer, buf->data(), K_CHUNK_VOLUME * sizeof(VoxelCell));
        }
    } else if (tmpGrid.hasChunk(cx, cy, cz)) {
        // Sentinel chunk (e.g. fillChunk for homogeneous stone)
        auto fill = tmpGrid.getChunkFillValue(cx, cy, cz);
        std::fill(buffer, buffer + K_CHUNK_VOLUME, fill);
    }
    // else: chunk not added by generator, buffer stays zero-initialized (air)
}

uint16_t WorldGenerator::sampleMaterial(int /*wx*/, int /*wy*/, int /*wz*/) const {
    return 0; // AIR; subclasses override with efficient point queries
}

int WorldGenerator::maxSurfaceHeight(int /*cx*/, int /*cz*/) const {
    return 1024; // Conservative default: never skip for unknown generators
}

std::string WorldGenerator::worldgenFingerprintSource() const {
    return name();
}

uint32_t WorldGenerator::worldgenFingerprint() const {
    return hashFNV1a(worldgenFingerprintSource());
}

} // namespace recurse
