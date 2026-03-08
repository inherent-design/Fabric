#pragma once

#include "fabric/core/BgfxHandle.hh"
#include <unordered_map>

namespace fabric {

// RAII map of keyed bgfx handles. Destructor calls destroyAll().
// Move-only; deleted copy.
//
// Usage:
//   HandleMap<ChunkKey, bgfx::DynamicVertexBufferHandle> buffers;
//   buffers.emplace(key, bgfx::createDynamicVertexBuffer(...));
//   buffers.erase(key);  // handle destroyed automatically
template <typename K, typename T> class HandleMap {
  public:
    using iterator = typename std::unordered_map<K, BgfxHandle<T>>::iterator;
    using const_iterator = typename std::unordered_map<K, BgfxHandle<T>>::const_iterator;

    HandleMap() = default;
    ~HandleMap() { destroyAll(); }

    HandleMap(HandleMap&&) noexcept = default;
    HandleMap& operator=(HandleMap&&) noexcept = default;
    HandleMap(const HandleMap&) = delete;
    HandleMap& operator=(const HandleMap&) = delete;

    // Insert or replace. If key exists, old handle is destroyed first.
    // Returns true if a new entry was created, false if an existing entry was replaced.
    bool emplace(const K& key, T rawHandle) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second.reset(rawHandle);
            return false;
        }
        map_.emplace(key, BgfxHandle<T>(rawHandle));
        return true;
    }

    // Replace handle at key. Destroys old handle if key exists; inserts if not.
    void replaceHandle(const K& key, T newHandle) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second.reset(newHandle);
        } else {
            map_.emplace(key, BgfxHandle<T>(newHandle));
        }
    }

    // Remove entry and destroy its handle. Returns true if key was found.
    bool erase(const K& key) { return map_.erase(key) > 0; }

    // Destroy all handles and clear the map.
    void destroyAll() { map_.clear(); }

    // Find handle by key. Returns nullptr if not found.
    BgfxHandle<T>* find(const K& key) {
        auto it = map_.find(key);
        return it != map_.end() ? &it->second : nullptr;
    }

    const BgfxHandle<T>* find(const K& key) const {
        auto it = map_.find(key);
        return it != map_.end() ? &it->second : nullptr;
    }

    // Get raw handle by key. Returns invalid handle if not found.
    T get(const K& key) const {
        auto it = map_.find(key);
        if (it != map_.end()) {
            return it->second.get();
        }
        T invalid;
        invalid.idx = bgfx::kInvalidHandle;
        return invalid;
    }

    bool contains(const K& key) const { return map_.contains(key); }

    size_t size() const noexcept { return map_.size(); }
    bool empty() const noexcept { return map_.empty(); }

    iterator begin() noexcept { return map_.begin(); }
    iterator end() noexcept { return map_.end(); }
    const_iterator begin() const noexcept { return map_.begin(); }
    const_iterator end() const noexcept { return map_.end(); }

  private:
    std::unordered_map<K, BgfxHandle<T>> map_;
};

} // namespace fabric
