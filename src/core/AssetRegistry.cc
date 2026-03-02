#include "fabric/core/AssetRegistry.hh"

namespace fabric {

AssetRegistry::AssetRegistry(ResourceHub& hub) : hub_(hub) {}

AssetRegistry::~AssetRegistry() = default;

size_t AssetRegistry::totalCount() const {
    std::lock_guard lock(registryMutex_);
    size_t total = 0;
    for (auto& [idx, slot] : types_) {
        if (slot.countFn)
            total += slot.countFn();
    }
    return total;
}

void AssetRegistry::update() {
    std::lock_guard lock(registryMutex_);
    ++currentTick_;
    for (auto& [idx, slot] : types_) {
        if (slot.evictFn)
            slot.evictFn(currentTick_);
    }
}

} // namespace fabric
