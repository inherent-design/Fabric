#pragma once

#include "fabric/core/VoxelVertex.hh"

#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace fabric {

struct PoolSlot {
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t bucketId = UINT32_MAX;

    bool valid() const { return bucketId != UINT32_MAX; }
};

class VertexPool {
  public:
    struct Config {
        uint32_t maxVerticesPerBucket = 16 * 1024; // 16K vertices per slot
        uint32_t maxIndicesPerBucket = 24 * 1024;  // 24K indices per slot
        uint32_t initialBuckets = 256;
        bool cpuOnly = false; // Skip bgfx operations (for unit testing)
    };

    VertexPool() = default;
    ~VertexPool();

    VertexPool(const VertexPool&) = delete;
    VertexPool& operator=(const VertexPool&) = delete;

    void init();
    void init(const Config& config);
    void shutdown();
    bool isValid() const;

    // Allocate a bucket and upload mesh data. Returns invalid slot on failure.
    PoolSlot allocate(const VoxelVertex* vertices, uint32_t vertexCount, const uint32_t* indices, uint32_t indexCount);

    // Return a bucket to the free list.
    void free(const PoolSlot& slot);

    bgfx::DynamicVertexBufferHandle vertexBuffer() const;
    bgfx::DynamicIndexBufferHandle indexBuffer() const;

    uint32_t allocatedBuckets() const;
    uint32_t totalBuckets() const;
    uint32_t maxVerticesPerBucket() const;
    uint32_t maxIndicesPerBucket() const;

  private:
    Config config_;
    bool initialized_ = false;
    bgfx::DynamicVertexBufferHandle vbh_ = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle ibh_ = BGFX_INVALID_HANDLE;

    std::vector<uint32_t> freeList_; // Stack of free bucket IDs
    std::vector<bool> used_;         // Which buckets are in use
    uint32_t allocatedCount_ = 0;
};

} // namespace fabric
