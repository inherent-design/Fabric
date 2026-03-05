#pragma once

#include "recurse/world/SmoothVoxelVertex.hh"

#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace recurse {

struct SmoothPoolSlot {
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t bucketId = UINT32_MAX;

    bool valid() const { return bucketId != UINT32_MAX; }
};

class SmoothVertexPool {
  public:
    struct Config {
        uint32_t maxVerticesPerBucket = 4 * 1024; // 4K * 32B = 128KB
        uint32_t maxIndicesPerBucket = 6 * 1024;  // 6K * 4B = 24KB
        uint32_t initialBuckets = 256;
        bool cpuOnly = false; // Skip bgfx operations (for unit testing)
    };

    SmoothVertexPool() = default;
    ~SmoothVertexPool();

    SmoothVertexPool(const SmoothVertexPool&) = delete;
    SmoothVertexPool& operator=(const SmoothVertexPool&) = delete;

    void init();
    void init(const Config& config);
    void shutdown();
    bool isValid() const;

    // Allocate a bucket and upload mesh data. Returns invalid slot on failure.
    SmoothPoolSlot allocate(const SmoothVoxelVertex* vertices, uint32_t vertexCount, const uint32_t* indices,
                            uint32_t indexCount);

    // Return a bucket to the free list.
    void free(const SmoothPoolSlot& slot);

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

} // namespace recurse
