#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <cstdint>
#include <string>
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

/// Filesystem-backed ChunkStore. One file per chunk.
///
/// Directory layout:
///   {worldDir}/chunks/gen/cx_cy_cz.fchk    (initial generation cache)
///   {worldDir}/chunks/delta/cx_cy_cz.fchk  (player modifications)
class FilesystemChunkStore : public ChunkStore {
  public:
    explicit FilesystemChunkStore(const std::string& worldDir);

    bool hasGenData(int cx, int cy, int cz) const override;
    std::optional<ChunkBlob> loadGenData(int cx, int cy, int cz) const override;
    void saveGenData(int cx, int cy, int cz, const ChunkBlob& data) override;

    bool hasDelta(int cx, int cy, int cz) const override;
    std::optional<ChunkBlob> loadDelta(int cx, int cy, int cz) const override;
    void saveDelta(int cx, int cy, int cz, const ChunkBlob& data) override;

    void compactChunk(int cx, int cy, int cz) override;

    size_t deltaSize(int cx, int cy, int cz) const override;
    size_t genDataSize(int cx, int cy, int cz) const override;

    /// Encode raw VoxelCell data into FCHK v2 blob.
    /// When paletteData is non-null and paletteEntryCount > 0, palette is appended after the
    /// voxel payload. Compression wraps bytes 10-EOF (payload + palette).
    static ChunkBlob encode(const void* cells, size_t cellsByteCount, uint8_t compression = 0, int level = 1,
                            const float* paletteData = nullptr, uint16_t paletteEntryCount = 0);

    /// Decode FCHK blob (v1 or v2). Validates header, decompresses if needed.
    /// v1 files: returns cells only, paletteEntryCount == 0, essenceIdx bytes zeroed.
    /// v2 files: returns cells + palette section.
    static FchkDecoded decode(const ChunkBlob& blob);

  private:
    std::string genPath(int cx, int cy, int cz) const;
    std::string deltaPath(int cx, int cy, int cz) const;
    std::optional<ChunkBlob> loadFile(const std::string& path) const;
    void saveFile(const std::string& path, const ChunkBlob& data);
    size_t fileSize(const std::string& path) const;

    std::string genDir_;
    std::string deltaDir_;
};

} // namespace recurse
