#pragma once

#include "fabric/utils/ErrorHandling.hh"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace fabric {

class BufferPool;

// Move-only RAII handle for a borrowed buffer slot.
// Automatically returns the slot to the pool on destruction.
class BufferSlot {
  public:
    BufferSlot() : pool_(nullptr), data_(nullptr), size_(0) {}
    ~BufferSlot();

    BufferSlot(BufferSlot&& other) noexcept : pool_(other.pool_), data_(other.data_), size_(other.size_) {
        other.pool_ = nullptr;
        other.data_ = nullptr;
        other.size_ = 0;
    }

    BufferSlot& operator=(BufferSlot&& other) noexcept {
        if (this != &other) {
            release();
            pool_ = other.pool_;
            data_ = other.data_;
            size_ = other.size_;
            other.pool_ = nullptr;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    BufferSlot(const BufferSlot&) = delete;
    BufferSlot& operator=(const BufferSlot&) = delete;

    uint8_t* data() { return data_; }
    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    std::span<uint8_t> span() { return {data_, size_}; }
    std::span<const uint8_t> span() const { return {data_, size_}; }

    explicit operator bool() const { return data_ != nullptr; }

  private:
    friend class BufferPool;
    BufferSlot(BufferPool* pool, uint8_t* data, size_t size) : pool_(pool), data_(data), size_(size) {}

    void release();

    BufferPool* pool_;
    uint8_t* data_;
    size_t size_;
};

// Fixed-size slab allocator with borrow/return semantics.
// All memory is pre-allocated in a single contiguous block.
// Thread-safe: blocking borrow() and non-blocking tryBorrow().
class BufferPool {
  public:
    BufferPool(size_t slotSize, size_t slotCount) : slotSize_(slotSize), slotCount_(slotCount) {
        storage_.resize(slotSize * slotCount);
        freeSlots_.reserve(slotCount);
        for (size_t i = slotCount; i > 0; --i) {
            freeSlots_.push_back(storage_.data() + (i - 1) * slotSize);
        }
    }

    // Blocking borrow: waits until a slot is available.
    BufferSlot borrow() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !freeSlots_.empty(); });
        return popSlot();
    }

    // Non-blocking borrow: returns nullopt if pool is exhausted.
    std::optional<BufferSlot> tryBorrow() {
        std::lock_guard lock(mutex_);
        if (freeSlots_.empty()) {
            return std::nullopt;
        }
        return popSlot();
    }

    size_t available() const {
        std::lock_guard lock(mutex_);
        return freeSlots_.size();
    }

    size_t capacity() const { return slotCount_; }
    size_t slotSize() const { return slotSize_; }

  private:
    friend class BufferSlot;

    BufferSlot popSlot() {
        uint8_t* ptr = freeSlots_.back();
        freeSlots_.pop_back();
        return BufferSlot(this, ptr, slotSize_);
    }

    void returnSlot(uint8_t* ptr) {
        {
            std::lock_guard lock(mutex_);
            freeSlots_.push_back(ptr);
        }
        cv_.notify_one();
    }

    size_t slotSize_;
    size_t slotCount_;
    std::vector<uint8_t> storage_;
    std::vector<uint8_t*> freeSlots_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

// Inline definitions for BufferSlot that depend on BufferPool
inline BufferSlot::~BufferSlot() {
    release();
}

inline void BufferSlot::release() {
    if (pool_ && data_) {
        pool_->returnSlot(data_);
        pool_ = nullptr;
        data_ = nullptr;
        size_ = 0;
    }
}

} // namespace fabric
