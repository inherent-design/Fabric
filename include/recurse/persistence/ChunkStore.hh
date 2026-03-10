#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace recurse {

/// Raw binary chunk data (gen snapshot or delta snapshot).
using ChunkBlob = std::vector<uint8_t>;

/// Abstract chunk storage backend. FilesystemChunkStore is the first
/// implementation; pgSQL or network backends can follow.
class ChunkStore {
  public:
    virtual ~ChunkStore() = default;

    /// Gen data: cached initial terrain generation output.
    virtual bool hasGenData(int cx, int cy, int cz) const = 0;
    virtual std::optional<ChunkBlob> loadGenData(int cx, int cy, int cz) const = 0;
    virtual void saveGenData(int cx, int cy, int cz, const ChunkBlob& data) = 0;

    /// Delta data: player modifications as full-chunk snapshot.
    virtual bool hasDelta(int cx, int cy, int cz) const = 0;
    virtual std::optional<ChunkBlob> loadDelta(int cx, int cy, int cz) const = 0;
    virtual void saveDelta(int cx, int cy, int cz, const ChunkBlob& data) = 0;

    /// Merge delta into gen data (replaces gen file, deletes delta file).
    virtual void compactChunk(int cx, int cy, int cz) = 0;

    /// Size queries for compaction threshold decisions.
    virtual size_t deltaSize(int cx, int cy, int cz) const = 0;
    virtual size_t genDataSize(int cx, int cy, int cz) const = 0;
};

} // namespace recurse
