#include "fabric/core/VertexPool.hh"

#include "fabric/core/VoxelMesher.hh"
#include "fabric/utils/ErrorHandling.hh"

namespace fabric {

VertexPool::~VertexPool() {
    shutdown();
}

void VertexPool::init() {
    init(Config{});
}

void VertexPool::init(const Config& config) {
    if (initialized_)
        return;

    config_ = config;

    // Build free list: push in reverse so bucket 0 is popped first
    freeList_.reserve(config_.initialBuckets);
    for (uint32_t i = config_.initialBuckets; i > 0; --i) {
        freeList_.push_back(i - 1);
    }
    used_.resize(config_.initialBuckets, false);
    allocatedCount_ = 0;

    if (!config_.cpuOnly) {
        uint32_t totalVertices = config_.maxVerticesPerBucket * config_.initialBuckets;
        uint32_t totalIndices = config_.maxIndicesPerBucket * config_.initialBuckets;

        vbh_ = bgfx::createDynamicVertexBuffer(totalVertices, VoxelMesher::getVertexLayout(), BGFX_BUFFER_ALLOW_RESIZE);
        ibh_ = bgfx::createDynamicIndexBuffer(totalIndices, BGFX_BUFFER_INDEX32 | BGFX_BUFFER_ALLOW_RESIZE);
    }

    initialized_ = true;
}

void VertexPool::shutdown() {
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

bool VertexPool::isValid() const {
    return initialized_;
}

PoolSlot VertexPool::allocate(const VoxelVertex* vertices, uint32_t vertexCount, const uint32_t* indices,
                              uint32_t indexCount) {
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
                     bgfx::copy(vertices, vertexCount * static_cast<uint32_t>(sizeof(VoxelVertex))));
        bgfx::update(ibh_, slot.indexOffset, bgfx::copy(indices, indexCount * static_cast<uint32_t>(sizeof(uint32_t))));
    }

    return slot;
}

void VertexPool::free(const PoolSlot& slot) {
    if (!initialized_ || !slot.valid())
        return;
    if (slot.bucketId >= used_.size() || !used_[slot.bucketId])
        return;

    used_[slot.bucketId] = false;
    freeList_.push_back(slot.bucketId);
    --allocatedCount_;
}

bgfx::DynamicVertexBufferHandle VertexPool::vertexBuffer() const {
    return vbh_;
}

bgfx::DynamicIndexBufferHandle VertexPool::indexBuffer() const {
    return ibh_;
}

uint32_t VertexPool::allocatedBuckets() const {
    return allocatedCount_;
}

uint32_t VertexPool::totalBuckets() const {
    return initialized_ ? config_.initialBuckets : 0;
}

uint32_t VertexPool::maxVerticesPerBucket() const {
    return initialized_ ? config_.maxVerticesPerBucket : 0;
}

uint32_t VertexPool::maxIndicesPerBucket() const {
    return initialized_ ? config_.maxIndicesPerBucket : 0;
}

} // namespace fabric
