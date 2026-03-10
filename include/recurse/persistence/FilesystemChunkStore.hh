#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <cstdint>
#include <string>

namespace recurse {

/// FCHK binary chunk format header (10 bytes).
struct FchkHeader {
    char magic[4]{'F', 'C', 'H', 'K'};
    uint16_t version{1};
    uint8_t dimX{32};
    uint8_t dimY{32};
    uint8_t dimZ{32};
    uint8_t compression{0}; // 0 = none, 1 = zstd (future), 2 = lz4 (future)
};
static_assert(sizeof(FchkHeader) == 10, "FchkHeader must be 10 bytes (packed)");

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

    /// Encode raw VoxelCell data into FCHK blob (header + payload).
    static ChunkBlob encode(const void* cells, size_t byteCount);

    /// Decode FCHK blob, returning pointer to payload and its size.
    /// Validates header. Throws on format mismatch.
    static std::pair<const uint8_t*, size_t> decodeView(const ChunkBlob& blob);

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
