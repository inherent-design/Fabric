#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <cstdint>
#include <vector>

namespace recurse {

/// FCHK binary chunk format header (10 bytes).
struct FchkHeader {
    char magic[4]{'F', 'C', 'H', 'K'};
    uint16_t version{4};
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

/// Sparse delta entry from v3 decode.
struct FchkDeltaEntry {
    uint32_t cellIndex;
    uint32_t cellData;
};
static_assert(sizeof(FchkDeltaEntry) == 8, "FchkDeltaEntry must be 8 bytes for binary format");

/// Decoded FCHK v3 delta payload.
struct FchkDeltaDecoded {
    uint32_t worldgenVersion{0};
    std::vector<FchkDeltaEntry> entries;
    std::vector<float> paletteData;
    uint16_t paletteEntryCount{0};
};

/// Shared FCHK codec. Handles v1/v2/v4 full blobs and v3 delta blobs.
struct FchkCodec {
    /// Encode raw cell data into FCHK v2 blob (v4 encode added in W4-D).
    /// When paletteData is non-null and paletteEntryCount > 0, palette is appended after the
    /// voxel payload. Compression wraps bytes 10-EOF (payload + palette).
    static ChunkBlob encode(const void* cells, size_t cellsByteCount, uint8_t compression = 0, int level = 1,
                            const float* paletteData = nullptr, uint16_t paletteEntryCount = 0);

    /// Decode FCHK blob (v1, v2, or v4). Validates header, decompresses if needed.
    /// v1 files: returns cells only, paletteEntryCount == 0, essenceIdx bytes zeroed.
    /// v2 files: returns cells + palette section.
    /// v4 files: MatterState layout, runtime flags cleared from phaseAndFlags byte.
    /// Throws for v3 blobs (use decodeDelta instead).
    static FchkDecoded decode(const ChunkBlob& blob);

    /// Encode delta between current and reference cell buffers as FCHK v3 blob.
    /// Only cells where current != reference are stored.
    /// worldgenVersion: caller-provided hash for version tracking.
    static ChunkBlob encodeDelta(const void* currentCells, const void* referenceCells, size_t cellsByteCount,
                                 uint32_t worldgenVersion, uint8_t compression = 1, int level = 1,
                                 const float* paletteData = nullptr, uint16_t paletteEntryCount = 0);

    /// Decode a v3 delta blob. Throws if blob is not v3.
    static FchkDeltaDecoded decodeDelta(const ChunkBlob& blob);

    /// Check if a blob is a v3 delta format (without full decode).
    static bool isDelta(const ChunkBlob& blob);

    /// Decode any FCHK blob (v1/v2/v3/v4). For v3 deltas, applies diff entries
    /// to refCells to produce full cell data. refCells must point to
    /// K_CHUNK_VOLUME * sizeof(VoxelCell) bytes when blob is v3.
    /// For v1/v2, refCells is ignored. Throws if blob is v3 and refCells is null.
    static FchkDecoded decodeAny(const ChunkBlob& blob, const void* refCells = nullptr);
};

} // namespace recurse
