#pragma once

#include "recurse/world/SmoothVoxelVertex.hh"
#include "recurse/world/VoxelVertex.hh"

#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace recurse {

// Generic pool slot returned from VertexPool::allocate()
struct PoolSlot {
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t bucketId = UINT32_MAX;

    bool valid() const { return bucketId != UINT32_MAX; }
};

// Traits: specialize to configure bucket sizes per vertex type
template <typename VertexT> struct VertexPoolTraits;

template <> struct VertexPoolTraits<VoxelVertex> {
    static constexpr uint32_t K_VERTEX_BUCKET_SIZE = 16 * 1024;
    static constexpr uint32_t K_INDEX_BUCKET_SIZE = 24 * 1024;
};

template <> struct VertexPoolTraits<SmoothVoxelVertex> {
    static constexpr uint32_t K_VERTEX_BUCKET_SIZE = 4 * 1024;
    static constexpr uint32_t K_INDEX_BUCKET_SIZE = 6 * 1024;
};

template <typename VertexT, typename Traits = VertexPoolTraits<VertexT>> class VertexPool {
  public:
    struct Config {
        uint32_t maxVerticesPerBucket = Traits::K_VERTEX_BUCKET_SIZE;
        uint32_t maxIndicesPerBucket = Traits::K_INDEX_BUCKET_SIZE;
        uint32_t initialBuckets = 256;
        bool cpuOnly = false;
    };

    VertexPool() = default;
    ~VertexPool() { shutdown(); }

    VertexPool(const VertexPool&) = delete;
    VertexPool& operator=(const VertexPool&) = delete;

    void init() { init(Config{}); }

    void init(const Config& config) {
        if (initialized_)
            return;

        config_ = config;

        freeList_.reserve(config_.initialBuckets);
        for (uint32_t i = config_.initialBuckets; i > 0; --i) {
            freeList_.push_back(i - 1);
        }
        used_.resize(config_.initialBuckets, false);
        allocatedCount_ = 0;

        if (!config_.cpuOnly) {
            uint32_t totalVertices = config_.maxVerticesPerBucket * config_.initialBuckets;
            uint32_t totalIndices = config_.maxIndicesPerBucket * config_.initialBuckets;

            vbh_ = bgfx::createDynamicVertexBuffer(totalVertices, VertexT::getVertexLayout(), BGFX_BUFFER_ALLOW_RESIZE);
            ibh_ = bgfx::createDynamicIndexBuffer(totalIndices, BGFX_BUFFER_INDEX32 | BGFX_BUFFER_ALLOW_RESIZE);
        }

        initialized_ = true;
    }

    void shutdown() {
        if (!initialized_)
            return;

        if (!config_.cpuOnly) {
            if (bgfx::isValid(vbh_))
                bgfx::destroy(vbh_);
            if (bgfx::isValid(ibh_))
                bgfx::destroy(ibh_);
        }

        vbh_ = BGFX_INVALID_HANDLE;
        ibh_ = BGFX_INVALID_HANDLE;
        freeList_.clear();
        used_.clear();
        allocatedCount_ = 0;
        initialized_ = false;
    }

    bool isValid() const { return initialized_; }

    PoolSlot allocate(const VertexT* vertices, uint32_t vertexCount, const uint32_t* indices, uint32_t indexCount) {
        if (!initialized_ || freeList_.empty())
            return {};
        if (vertexCount > config_.maxVerticesPerBucket || indexCount > config_.maxIndicesPerBucket)
            return {};

        uint32_t bucketId = freeList_.back();
        freeList_.pop_back();
        used_[bucketId] = true;
        ++allocatedCount_;

        PoolSlot slot;
        slot.bucketId = bucketId;
        slot.vertexOffset = bucketId * config_.maxVerticesPerBucket;
        slot.indexOffset = bucketId * config_.maxIndicesPerBucket;
        slot.vertexCount = vertexCount;
        slot.indexCount = indexCount;

        if (!config_.cpuOnly && vertexCount > 0 && indexCount > 0) {
            bgfx::update(vbh_, slot.vertexOffset,
                         bgfx::copy(vertices, vertexCount * static_cast<uint32_t>(sizeof(VertexT))));
            bgfx::update(ibh_, slot.indexOffset,
                         bgfx::copy(indices, indexCount * static_cast<uint32_t>(sizeof(uint32_t))));
        }

        return slot;
    }

    void free(const PoolSlot& slot) {
        if (!initialized_ || !slot.valid())
            return;
        if (slot.bucketId >= used_.size() || !used_[slot.bucketId])
            return;

        used_[slot.bucketId] = false;
        freeList_.push_back(slot.bucketId);
        --allocatedCount_;
    }

    bgfx::DynamicVertexBufferHandle vertexBuffer() const { return vbh_; }
    bgfx::DynamicIndexBufferHandle indexBuffer() const { return ibh_; }

    uint32_t allocatedBuckets() const { return allocatedCount_; }
    uint32_t totalBuckets() const { return initialized_ ? config_.initialBuckets : 0; }
    uint32_t maxVerticesPerBucket() const { return initialized_ ? config_.maxVerticesPerBucket : 0; }
    uint32_t maxIndicesPerBucket() const { return initialized_ ? config_.maxIndicesPerBucket : 0; }

  private:
    Config config_;
    bool initialized_ = false;
    bgfx::DynamicVertexBufferHandle vbh_ = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle ibh_ = BGFX_INVALID_HANDLE;

    std::vector<uint32_t> freeList_;
    std::vector<bool> used_;
    uint32_t allocatedCount_ = 0;
};

// Type aliases for backward compatibility
using VoxelVertexPool = VertexPool<VoxelVertex>;
using SmoothVertexPool = VertexPool<SmoothVoxelVertex>;

// Backward-compatible slot alias
using SmoothPoolSlot = PoolSlot;

} // namespace recurse
