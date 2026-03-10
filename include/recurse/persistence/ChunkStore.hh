#pragma once

#include <cstdint>
#include <optional>
#include <tuple>
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

    /// Batch load gen data. Returns (coord, blob) pairs for chunks that exist.
    /// Default loops over loadGenData(). Backends may override for optimized I/O.
    virtual std::vector<std::pair<std::tuple<int, int, int>, ChunkBlob>>
    loadBatch(const std::vector<std::tuple<int, int, int>>& coords) const {
        std::vector<std::pair<std::tuple<int, int, int>, ChunkBlob>> results;
        results.reserve(coords.size());
        for (const auto& [cx, cy, cz] : coords) {
            auto blob = loadGenData(cx, cy, cz);
            if (blob)
                results.push_back({{cx, cy, cz}, std::move(*blob)});
        }
        return results;
    }

    /// Batch save gen data. Default loops over saveGenData().
    /// Backends may override for sorted sequential I/O or region-file batching.
    virtual void saveBatch(const std::vector<std::pair<std::tuple<int, int, int>, ChunkBlob>>& entries) {
        for (const auto& [coord, blob] : entries) {
            auto [cx, cy, cz] = coord;
            saveGenData(cx, cy, cz, blob);
        }
    }
};

} // namespace recurse
