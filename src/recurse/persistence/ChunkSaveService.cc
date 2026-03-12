#include "recurse/persistence/ChunkSaveService.hh"

#include "fabric/platform/JobScheduler.hh"
#include <algorithm>
#include <vector>

namespace recurse {

ChunkSaveService::ChunkSaveService(ChunkStore& store, fabric::JobScheduler& jobs, DataProvider provider)
    : store_(store), jobs_(jobs), provider_(std::move(provider)) {}

void ChunkSaveService::markDirty(int cx, int cy, int cz) {
    std::lock_guard lock(mutex_);
    auto key = makeKey(cx, cy, cz);
    auto it = dirty_.find(key);
    if (it == dirty_.end()) {
        dirty_[key] = DirtyEntry{0.0f, 0.0f, false};
    } else {
        it->second.lastDirtyAge = 0.0f; // reset debounce
    }
}

void ChunkSaveService::update(float dt) {
    std::vector<std::tuple<int, int, int>> toSave;

    {
        std::lock_guard lock(mutex_);
        for (auto& [key, entry] : dirty_) {
            if (entry.saving)
                continue;

            entry.firstDirtyAge += dt;
            entry.lastDirtyAge += dt;

            bool debounceExpired = entry.lastDirtyAge >= debounceSeconds;
            bool maxDelayExpired = entry.firstDirtyAge >= maxDelaySeconds;

            if (debounceExpired || maxDelayExpired) {
                // Decode key back to coordinates
                int cz = static_cast<int>(key & 0x1FFFFF);
                if (cz & 0x100000)
                    cz |= ~0x1FFFFF; // sign extend
                int cy = static_cast<int>((key >> 21) & 0x1FFFFF);
                if (cy & 0x100000)
                    cy |= ~0x1FFFFF;
                int cx = static_cast<int>((key >> 42) & 0x1FFFFF);
                if (cx & 0x100000)
                    cx |= ~0x1FFFFF;

                entry.saving = true;
                toSave.emplace_back(cx, cy, cz);
            }
        }
    }

    std::sort(toSave.begin(), toSave.end());
    for (auto& [cx, cy, cz] : toSave) {
        saveChunk(cx, cy, cz);
    }
}

void ChunkSaveService::flush() {
    std::vector<std::tuple<int, int, int>> toSave;

    {
        std::lock_guard lock(mutex_);
        for (auto& [key, entry] : dirty_) {
            int cz = static_cast<int>(key & 0x1FFFFF);
            if (cz & 0x100000)
                cz |= ~0x1FFFFF;
            int cy = static_cast<int>((key >> 21) & 0x1FFFFF);
            if (cy & 0x100000)
                cy |= ~0x1FFFFF;
            int cx = static_cast<int>((key >> 42) & 0x1FFFFF);
            if (cx & 0x100000)
                cx |= ~0x1FFFFF;

            toSave.emplace_back(cx, cy, cz);
        }
    }

    std::sort(toSave.begin(), toSave.end());
    for (auto& [cx, cy, cz] : toSave) {
        saveChunkSync(cx, cy, cz);
    }

    std::lock_guard lock(mutex_);
    dirty_.clear();
}

size_t ChunkSaveService::pendingCount() const {
    std::lock_guard lock(mutex_);
    return dirty_.size();
}

ChunkSaveService::ChunkKey ChunkSaveService::makeKey(int cx, int cy, int cz) {
    // Pack three 21-bit signed integers into int64_t
    auto pack = [](int v) -> int64_t {
        return static_cast<int64_t>(v) & 0x1FFFFF;
    };
    return (pack(cx) << 42) | (pack(cy) << 21) | pack(cz);
}

void ChunkSaveService::saveChunk(int cx, int cy, int cz) {
    jobs_.submitBackground([this, cx, cy, cz]() {
        saveChunkSync(cx, cy, cz);

        std::lock_guard lock(mutex_);
        auto key = makeKey(cx, cy, cz);
        dirty_.erase(key);
    });
}

void ChunkSaveService::saveChunkSync(int cx, int cy, int cz) {
    auto blob = provider_(cx, cy, cz);
    if (blob.empty())
        return;

    store_.saveChunk(cx, cy, cz, blob);
}

} // namespace recurse
