#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <cstdint>
#include <vector>

namespace recurse {

/// FCHK binary chunk format header (10 bytes).
struct FchkHeader {
    char magic[4]{'F', 'C', 'H', 'K'};
    uint16_t version{2};
    uint8_t dimX{32};
    uint8_t dimY{32};
    uint8_t dimZ{32};
    uint8_t compression{0}; // 0 = none, 1 = zstd, 2 = lz4
};
static_assert(sizeof(FchkHeader) == 10, "FchkHeader must be 10 bytes (packed)");

/// Decoded FCHK payload. v1 files have no palette (paletteEntryCount == 0).
struct FchkDecoded {
    std::vector<uint8_t> cells;     ///< Raw voxel cell bytes.
    std::vector<float> paletteData; ///< Flat float array (N * 4 floats: order, chaos, life, decay).
    uint16_t paletteEntryCount{0};  ///< Number of palette entries (0 for v1).
};

/// Shared FCHK v2 codec. Used by FilesystemChunkStore, SqliteChunkStore, and tests.
struct FchkCodec {
    /// Encode raw VoxelCell data into FCHK v2 blob.
    /// When paletteData is non-null and paletteEntryCount > 0, palette is appended after the
    /// voxel payload. Compression wraps bytes 10-EOF (payload + palette).
    static ChunkBlob encode(const void* cells, size_t cellsByteCount, uint8_t compression = 0, int level = 1,
                            const float* paletteData = nullptr, uint16_t paletteEntryCount = 0);

    /// Decode FCHK blob (v1 or v2). Validates header, decompresses if needed.
    /// v1 files: returns cells only, paletteEntryCount == 0, essenceIdx bytes zeroed.
    /// v2 files: returns cells + palette section.
    static FchkDecoded decode(const ChunkBlob& blob);
};

} // namespace recurse
