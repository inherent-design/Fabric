#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "fabric/utils/ErrorHandling.hh"

namespace fabric {

enum class AssetState : uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Failed
};

/// Type-erased base for shared state blocks.
struct AssetStateBlockBase {
    std::atomic<AssetState> state{AssetState::Unloaded};
    std::string path;
    std::string error;
    std::atomic<uint64_t> lastAccessTick{0};

    virtual ~AssetStateBlockBase() = default;
    virtual size_t memoryUsage() const = 0;
};

/// Typed shared state block containing the actual asset data.
/// Shared between Handle<T> instances and the AssetRegistry.
template <typename T> struct AssetStateBlock : AssetStateBlockBase {
    std::unique_ptr<T> asset;
    std::vector<std::function<void(T&)>> onLoadedCallbacks;
    std::mutex callbackMutex;

    size_t memoryUsage() const override {
        return (state.load(std::memory_order_acquire) == AssetState::Loaded && asset) ? sizeof(T) : 0;
    }

    void notifyLoaded() {
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (asset) {
            for (auto& cb : onLoadedCallbacks) {
                cb(*asset);
            }
        }
        onLoadedCallbacks.clear();
    }

    void notifyFailed() {
        std::lock_guard<std::mutex> lock(callbackMutex);
        onLoadedCallbacks.clear();
    }
};

/// Lightweight, copyable, ref-counted handle to a typed asset.
/// Internally holds a shared_ptr to an AssetStateBlock<T>.
template <typename T> class Handle {
  public:
    Handle() = default;

    /// Get the asset. Throws if not loaded.
    T& get() const {
        if (!block_)
            throwError("Handle::get() called on null handle");
        auto s = block_->state.load(std::memory_order_acquire);
        if (s != AssetState::Loaded || !block_->asset)
            throwError("Handle::get() called on non-loaded asset (path: " + block_->path + ")");
        return *block_->asset;
    }

    /// Get the asset or nullptr if not loaded.
    T* tryGet() const {
        if (!block_ || block_->state.load(std::memory_order_acquire) != AssetState::Loaded)
            return nullptr;
        return block_->asset.get();
    }

    bool isLoaded() const { return block_ && block_->state.load(std::memory_order_acquire) == AssetState::Loaded; }

    bool isFailed() const { return block_ && block_->state.load(std::memory_order_acquire) == AssetState::Failed; }

    AssetState state() const { return block_ ? block_->state.load(std::memory_order_acquire) : AssetState::Unloaded; }

    /// Register callback for when asset finishes loading.
    /// If already loaded, fires immediately.
    void onLoaded(std::function<void(T&)> callback) {
        if (!block_)
            return;
        if (block_->state.load(std::memory_order_acquire) == AssetState::Loaded && block_->asset) {
            callback(*block_->asset);
            return;
        }
        std::lock_guard<std::mutex> lock(block_->callbackMutex);
        if (block_->state.load(std::memory_order_acquire) == AssetState::Loaded && block_->asset) {
            callback(*block_->asset);
        } else {
            block_->onLoadedCallbacks.push_back(std::move(callback));
        }
    }

    bool operator==(const Handle& other) const { return block_ == other.block_; }
    bool operator!=(const Handle& other) const { return block_ != other.block_; }
    explicit operator bool() const { return block_ != nullptr; }

    const std::string& path() const {
        static const std::string empty;
        return block_ ? block_->path : empty;
    }

    const std::string& error() const {
        static const std::string empty;
        return block_ ? block_->error : empty;
    }

  private:
    friend class AssetRegistry;
    explicit Handle(std::shared_ptr<AssetStateBlock<T>> block) : block_(std::move(block)) {}
    std::shared_ptr<AssetStateBlock<T>> block_;
};

} // namespace fabric
