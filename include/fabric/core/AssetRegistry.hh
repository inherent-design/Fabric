#pragma once

#include "fabric/core/AssetLoader.hh"
#include "fabric/core/Handle.hh"
#include "fabric/core/Log.hh"

#include <algorithm>
#include <any>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace fabric {

class ResourceHub;

/// Typed asset manager with path-based deduplication, LRU eviction, and
/// per-type memory budgets. Type-erased internally, type-safe externally.
/// Loading is synchronous in this implementation; async via ResourceHub
/// worker pool is planned for a future sprint.
class AssetRegistry {
  public:
    explicit AssetRegistry(ResourceHub& hub);
    ~AssetRegistry();

    /// Register a loader for asset type T. Must be called before load<T>().
    template <typename T> void registerLoader(std::unique_ptr<AssetLoader<T>> loader);

    /// Load an asset by path. Returns existing handle if already loaded.
    /// Throws if no loader is registered for type T.
    template <typename T> Handle<T> load(const std::filesystem::path& path);

    /// Get an existing handle without triggering a load. Returns null handle
    /// if the path has never been loaded for this type.
    template <typename T> Handle<T> get(const std::filesystem::path& path) const;

    /// Unload by handle. The block transitions to Unloaded.
    template <typename T> void unload(const Handle<T>& handle);

    /// Unload by path.
    template <typename T> void unload(const std::filesystem::path& path);

    /// Reload an existing asset. Existing handles see the new data.
    template <typename T> void reload(const std::filesystem::path& path);

    /// Set per-type memory limit. When exceeded, LRU eviction runs in update().
    template <typename T> void setMemoryBudget(size_t bytes);

    /// Get current memory usage for a type.
    template <typename T> size_t getMemoryUsage() const;

    /// Number of loaded assets of type T.
    template <typename T> size_t count() const;

    /// Total number of loaded assets across all types.
    size_t totalCount() const;

    /// Per-frame processing: evict LRU assets over budget.
    /// Increments the internal tick counter used for LRU tracking.
    void update();

  private:
    ResourceHub& hub_;

    struct TypeSlot {
        std::any cache;
        std::any loader;
        std::function<size_t()> countFn;
        std::function<size_t()> memoryUsageFn;
        std::function<void(uint64_t)> evictFn;
        size_t memoryBudget = 0;
    };

    mutable std::mutex registryMutex_;
    std::unordered_map<std::type_index, TypeSlot> types_;
    std::unordered_map<std::string, std::type_index> extensionMap_;
    uint64_t currentTick_ = 0;
};

// -- Template implementations --

template <typename T> void AssetRegistry::registerLoader(std::unique_ptr<AssetLoader<T>> loader) {
    std::lock_guard lock(registryMutex_);
    auto idx = std::type_index(typeid(T));
    auto& slot = types_[idx];

    auto extensions = loader->extensions();
    slot.loader = std::shared_ptr<AssetLoader<T>>(std::move(loader));
    slot.cache = std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>{};

    slot.countFn = [&slot]() -> size_t {
        auto& cache = std::any_cast<std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>&>(slot.cache);
        size_t n = 0;
        for (auto& [p, block] : cache) {
            if (block->state.load(std::memory_order_acquire) == AssetState::Loaded)
                ++n;
        }
        return n;
    };

    slot.memoryUsageFn = [&slot]() -> size_t {
        auto& cache = std::any_cast<std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>&>(slot.cache);
        size_t total = 0;
        for (auto& [p, block] : cache) {
            total += block->memoryUsage();
        }
        return total;
    };

    // LRU eviction: remove oldest loaded assets until under budget
    slot.evictFn = [&slot](uint64_t /*tick*/) {
        if (slot.memoryBudget == 0)
            return;
        auto usage = slot.memoryUsageFn();
        if (usage <= slot.memoryBudget)
            return;

        auto& cache = std::any_cast<std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>&>(slot.cache);

        // Collect evictable entries: loaded and not held by external handles.
        // Check use_count before copying into the vector, when only the cache
        // holds the shared_ptr (use_count == 1 means no external handles).
        std::vector<std::pair<std::string, std::shared_ptr<AssetStateBlock<T>>>> evictable;
        for (auto& [p, block] : cache) {
            if (block->state.load(std::memory_order_acquire) == AssetState::Loaded && block.use_count() == 1)
                evictable.emplace_back(p, block);
        }
        std::sort(evictable.begin(), evictable.end(), [](const auto& a, const auto& b) {
            return a.second->lastAccessTick.load(std::memory_order_relaxed) <
                   b.second->lastAccessTick.load(std::memory_order_relaxed);
        });

        for (auto& [p, block] : evictable) {
            if (usage <= slot.memoryBudget)
                break;
            auto freed = block->memoryUsage();
            block->asset.reset();
            block->state.store(AssetState::Unloaded, std::memory_order_release);
            cache.erase(p);
            usage -= freed;
        }
    };

    for (auto& ext : extensions) {
        extensionMap_.emplace(ext, idx);
    }
}

template <typename T> Handle<T> AssetRegistry::load(const std::filesystem::path& path) {
    auto canonical = std::filesystem::weakly_canonical(path).string();

    std::shared_ptr<AssetStateBlock<T>> block;
    AssetLoader<T>* loaderPtr = nullptr;

    {
        std::lock_guard lock(registryMutex_);
        auto idx = std::type_index(typeid(T));
        auto it = types_.find(idx);
        if (it == types_.end())
            throwError("AssetRegistry::load: no loader registered for this type");

        auto& cache =
            std::any_cast<std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>&>(it->second.cache);

        auto cacheIt = cache.find(canonical);
        if (cacheIt != cache.end()) {
            cacheIt->second->lastAccessTick.store(currentTick_, std::memory_order_relaxed);
            return Handle<T>(cacheIt->second);
        }

        block = std::make_shared<AssetStateBlock<T>>();
        block->path = canonical;
        block->state.store(AssetState::Loading, std::memory_order_release);
        block->lastAccessTick.store(currentTick_, std::memory_order_relaxed);
        cache[canonical] = block;

        loaderPtr = std::any_cast<std::shared_ptr<AssetLoader<T>>&>(it->second.loader).get();
    }

    // Load outside the lock so other registry operations are not blocked
    try {
        block->asset = loaderPtr->load(std::filesystem::path(canonical), hub_);
        if (!block->asset) {
            block->error = "loader returned null";
            block->state.store(AssetState::Failed, std::memory_order_release);
            block->notifyFailed();
            FABRIC_LOG_ERROR("Asset load failed [{}]: loader returned null", canonical);
        } else {
            block->state.store(AssetState::Loaded, std::memory_order_release);
            block->notifyLoaded();
        }
    } catch (const std::exception& e) {
        block->error = e.what();
        block->state.store(AssetState::Failed, std::memory_order_release);
        block->notifyFailed();
        FABRIC_LOG_ERROR("Asset load failed [{}]: {}", canonical, e.what());
    }

    return Handle<T>(block);
}

template <typename T> Handle<T> AssetRegistry::get(const std::filesystem::path& path) const {
    auto canonical = std::filesystem::weakly_canonical(path).string();
    std::lock_guard lock(registryMutex_);

    auto idx = std::type_index(typeid(T));
    auto it = types_.find(idx);
    if (it == types_.end())
        return Handle<T>();

    auto& cache =
        std::any_cast<const std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>&>(it->second.cache);

    auto cacheIt = cache.find(canonical);
    if (cacheIt == cache.end())
        return Handle<T>();

    cacheIt->second->lastAccessTick.store(currentTick_, std::memory_order_relaxed);
    return Handle<T>(cacheIt->second);
}

template <typename T> void AssetRegistry::unload(const Handle<T>& handle) {
    if (!handle)
        return;
    unload<T>(std::filesystem::path(handle.path()));
}

template <typename T> void AssetRegistry::unload(const std::filesystem::path& path) {
    auto canonical = std::filesystem::weakly_canonical(path).string();
    std::lock_guard lock(registryMutex_);

    auto idx = std::type_index(typeid(T));
    auto it = types_.find(idx);
    if (it == types_.end())
        return;

    auto& cache =
        std::any_cast<std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>&>(it->second.cache);

    auto cacheIt = cache.find(canonical);
    if (cacheIt == cache.end())
        return;

    cacheIt->second->asset.reset();
    cacheIt->second->state.store(AssetState::Unloaded, std::memory_order_release);
    cache.erase(cacheIt);
}

template <typename T> void AssetRegistry::reload(const std::filesystem::path& path) {
    auto canonical = std::filesystem::weakly_canonical(path).string();

    std::shared_ptr<AssetStateBlock<T>> block;
    AssetLoader<T>* loaderPtr = nullptr;

    {
        std::lock_guard lock(registryMutex_);
        auto idx = std::type_index(typeid(T));
        auto it = types_.find(idx);
        if (it == types_.end())
            return;

        auto& cache =
            std::any_cast<std::unordered_map<std::string, std::shared_ptr<AssetStateBlock<T>>>&>(it->second.cache);

        auto cacheIt = cache.find(canonical);
        if (cacheIt == cache.end())
            return;

        block = cacheIt->second;
        block->state.store(AssetState::Loading, std::memory_order_release);
        loaderPtr = std::any_cast<std::shared_ptr<AssetLoader<T>>&>(it->second.loader).get();
    }

    try {
        block->asset = loaderPtr->load(std::filesystem::path(canonical), hub_);
        if (!block->asset) {
            block->error = "loader returned null";
            block->state.store(AssetState::Failed, std::memory_order_release);
            block->notifyFailed();
            FABRIC_LOG_ERROR("Asset reload failed [{}]: loader returned null", canonical);
        } else {
            block->state.store(AssetState::Loaded, std::memory_order_release);
            block->notifyLoaded();
        }
    } catch (const std::exception& e) {
        block->error = e.what();
        block->state.store(AssetState::Failed, std::memory_order_release);
        block->notifyFailed();
        FABRIC_LOG_ERROR("Asset reload failed [{}]: {}", canonical, e.what());
    }
}

template <typename T> void AssetRegistry::setMemoryBudget(size_t bytes) {
    std::lock_guard lock(registryMutex_);
    auto idx = std::type_index(typeid(T));
    auto it = types_.find(idx);
    if (it != types_.end())
        it->second.memoryBudget = bytes;
}

template <typename T> size_t AssetRegistry::getMemoryUsage() const {
    std::lock_guard lock(registryMutex_);
    auto idx = std::type_index(typeid(T));
    auto it = types_.find(idx);
    if (it == types_.end() || !it->second.memoryUsageFn)
        return 0;
    return it->second.memoryUsageFn();
}

template <typename T> size_t AssetRegistry::count() const {
    std::lock_guard lock(registryMutex_);
    auto idx = std::type_index(typeid(T));
    auto it = types_.find(idx);
    if (it == types_.end() || !it->second.countFn)
        return 0;
    return it->second.countFn();
}

} // namespace fabric
