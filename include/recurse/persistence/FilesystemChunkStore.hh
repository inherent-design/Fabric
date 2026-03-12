#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/FchkCodec.hh"
#include <string>

namespace recurse {

/// Filesystem-backed ChunkStore. One file per chunk.
///
/// Directory layout:
///   {worldDir}/chunks/gen/cx_cy_cz.fchk    (materialized chunk state)
class FilesystemChunkStore : public ChunkStore {
  public:
    explicit FilesystemChunkStore(const std::string& worldDir);

    // --- v2 API (primary) ---

    bool hasChunk(int cx, int cy, int cz) const override;
    std::optional<ChunkBlob> loadChunk(int cx, int cy, int cz) const override;
    void saveChunk(int cx, int cy, int cz, const ChunkBlob& data) override;
    size_t chunkSize(int cx, int cy, int cz) const override;

    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>
    loadBatch(const std::vector<fabric::ChunkCoord>& coords) const override;
    void saveBatch(const std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>& entries) override;

  private:
    std::string chunkPath(int cx, int cy, int cz) const;
    std::optional<ChunkBlob> loadFile(const std::string& path) const;
    void saveFile(const std::string& path, const ChunkBlob& data);
    size_t fileSize(const std::string& path) const;

    std::string chunkDir_;
};

} // namespace recurse
