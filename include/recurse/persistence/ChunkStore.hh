#pragma once

#include "fabric/world/ChunkCoord.hh"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace recurse {

/// Opaque compressed chunk blob (FCHK format with optional zstd compression).
struct ChunkBlob {
    std::vector<uint8_t> data;

    ChunkBlob() = default;
    explicit ChunkBlob(size_t n) : data(n) {}

    bool empty() const { return data.empty(); }
    size_t size() const { return data.size(); }
    uint8_t* data_ptr() { return data.data(); }
    const uint8_t* data_ptr() const { return data.data(); }
    void resize(size_t n) { data.resize(n); }
};

/// Abstract interface for chunk storage backends.
/// v2 API: single materialized state per chunk (no gen/delta split).
/// History is handled separately by WorldTransactionStore.
class ChunkStore {
  public:
    virtual ~ChunkStore() = default;

    /// Check if a chunk exists in storage.
    virtual bool hasChunk(int cx, int cy, int cz) const = 0;

    /// Load a chunk. Returns nullopt if not found.
    virtual std::optional<ChunkBlob> loadChunk(int cx, int cy, int cz) const = 0;

    /// Save a chunk. Empty blob may skip save (implementation-defined).
    virtual void saveChunk(int cx, int cy, int cz, const ChunkBlob& data) = 0;

    /// Get the stored size of a chunk in bytes. Returns 0 if not found.
    virtual size_t chunkSize(int cx, int cy, int cz) const = 0;

    /// Batch load multiple chunks. Returns only found chunks.
    virtual std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>
    loadBatch(const std::vector<fabric::ChunkCoord>& coords) const = 0;

    /// Batch save multiple chunks.
    virtual void saveBatch(const std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>& entries) = 0;
};

} // namespace recurse
