#include "recurse/persistence/ChunkSaveService.hh"

#include "fabric/log/Log.hh"
#include "fabric/platform/WriterQueue.hh"
#include <algorithm>
#include <exception>
#include <vector>

namespace recurse {

ChunkSaveService::ChunkSaveService(ChunkStore& store, fabric::platform::WriterQueue& writerQueue, DataProvider provider)
    : store_(store), writerQueue_(writerQueue), provider_(std::move(provider)) {}

ChunkSaveService::~ChunkSaveService() {
    writerQueue_.drain();
}

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

    if (!toSave.empty()) {
        std::sort(toSave.begin(), toSave.end());
        dispatchBatch(std::move(toSave));
    }
}

void ChunkSaveService::enqueuePrepared(int cx, int cy, int cz, ChunkBlob blob) {
    std::lock_guard lock(mutex_);
    preparedBlobs_.push_back({fabric::ChunkCoord{cx, cy, cz}, std::move(blob)});
    dirty_.erase(makeKey(cx, cy, cz));
}

void ChunkSaveService::flush() {
    writerQueue_.drain();

    std::vector<std::tuple<int, int, int>> toSave;
    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> prepared;
    uint64_t batchSerial = 0;

    {
        std::lock_guard lock(mutex_);
        batchSerial = ++nextBatchSerial_;
        lastStartedSerial_ = batchSerial;
        lastError_.clear();
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
        prepared = std::move(preparedBlobs_);
    }

    std::sort(toSave.begin(), toSave.end());

    try {
        FABRIC_LOG_INFO("ChunkSaveService: flush start serial={} dirty={} prepared={}", batchSerial, toSave.size(),
                        prepared.size());

        std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> entries;
        entries.reserve(toSave.size() + prepared.size());
        for (auto& [cx, cy, cz] : toSave) {
            auto blob = provider_(cx, cy, cz);
            if (!blob.empty()) {
                entries.push_back({fabric::ChunkCoord{cx, cy, cz}, std::move(blob)});
            }
        }
        for (const auto& entry : prepared)
            entries.push_back(entry);

        if (!entries.empty()) {
            store_.saveBatch(entries);
        }

        std::lock_guard lock(mutex_);
        dirty_.clear();
        lastCompletedSerial_ = batchSerial;
        lastSuccessfulSerial_ = batchSerial;
        lastError_.clear();
        FABRIC_LOG_INFO("ChunkSaveService: flush complete serial={} entries={}", batchSerial, entries.size());
    } catch (const std::exception& ex) {
        std::lock_guard lock(mutex_);
        lastCompletedSerial_ = batchSerial;
        lastError_ = ex.what();
        for (auto& entry : prepared)
            preparedBlobs_.push_back(entry);
        FABRIC_LOG_ERROR("ChunkSaveService: flush failed serial={}: {}", batchSerial, ex.what());
    } catch (...) {
        std::lock_guard lock(mutex_);
        lastCompletedSerial_ = batchSerial;
        lastError_ = "unknown error";
        for (auto& entry : prepared)
            preparedBlobs_.push_back(entry);
        FABRIC_LOG_ERROR("ChunkSaveService: flush failed serial={} with unknown error", batchSerial);
    }
}

size_t ChunkSaveService::pendingCount() const {
    std::lock_guard lock(mutex_);
    return dirty_.size();
}

ChunkSaveService::ActivitySnapshot ChunkSaveService::activitySnapshot() const {
    std::lock_guard lock(mutex_);

    ActivitySnapshot snapshot;
    snapshot.dirtyChunks = dirty_.size();
    snapshot.preparedChunks = preparedBlobs_.size();
    snapshot.lastStartedSerial = lastStartedSerial_;
    snapshot.lastCompletedSerial = lastCompletedSerial_;
    snapshot.lastSuccessfulSerial = lastSuccessfulSerial_;
    snapshot.hasError = !lastError_.empty();
    snapshot.lastError = lastError_;

    for (const auto& [_, entry] : dirty_) {
        if (entry.saving)
            ++snapshot.savingChunks;
    }

    return snapshot;
}

ChunkSaveService::ChunkKey ChunkSaveService::makeKey(int cx, int cy, int cz) {
    // Pack three 21-bit signed integers into int64_t
    auto pack = [](int v) -> int64_t {
        return static_cast<int64_t>(v) & 0x1FFFFF;
    };
    return (pack(cx) << 42) | (pack(cy) << 21) | pack(cz);
}

void ChunkSaveService::dispatchBatch(std::vector<std::tuple<int, int, int>> chunks) {
    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> prepared;
    uint64_t batchSerial = 0;
    {
        std::lock_guard lock(mutex_);
        prepared = std::move(preparedBlobs_);
        batchSerial = ++nextBatchSerial_;
        lastStartedSerial_ = batchSerial;
        lastError_.clear();
    }

    FABRIC_LOG_INFO("ChunkSaveService: dispatch batch serial={} dirty={} prepared={}", batchSerial, chunks.size(),
                    prepared.size());

    writerQueue_.submit([this, batch = std::move(chunks), prepared = std::move(prepared), batchSerial]() mutable {
        try {
            std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> entries;
            entries.reserve(batch.size() + prepared.size());

            for (auto& [cx, cy, cz] : batch) {
                auto blob = provider_(cx, cy, cz);
                if (!blob.empty() && blob.size() < 40)
                    continue; // F20: zero-diff blob; chunk matches worldgen reference
                if (!blob.empty()) {
                    entries.push_back({fabric::ChunkCoord{cx, cy, cz}, std::move(blob)});
                }
            }

            for (const auto& entry : prepared)
                entries.push_back(entry);

            if (!entries.empty()) {
                store_.saveBatch(entries);
            }

            std::lock_guard lock(mutex_);
            for (auto& [cx, cy, cz] : batch) {
                dirty_.erase(makeKey(cx, cy, cz));
            }
            lastCompletedSerial_ = batchSerial;
            lastSuccessfulSerial_ = batchSerial;
            lastError_.clear();
            FABRIC_LOG_INFO("ChunkSaveService: batch complete serial={} entries={}", batchSerial, entries.size());
        } catch (const std::exception& ex) {
            std::lock_guard lock(mutex_);
            lastCompletedSerial_ = batchSerial;
            lastError_ = ex.what();
            for (auto& [cx, cy, cz] : batch) {
                auto it = dirty_.find(makeKey(cx, cy, cz));
                if (it != dirty_.end())
                    it->second.saving = false;
            }
            for (auto& entry : prepared)
                preparedBlobs_.push_back(std::move(entry));
            FABRIC_LOG_ERROR("ChunkSaveService: batch failed serial={}: {}", batchSerial, ex.what());
        } catch (...) {
            std::lock_guard lock(mutex_);
            lastCompletedSerial_ = batchSerial;
            lastError_ = "unknown error";
            for (auto& [cx, cy, cz] : batch) {
                auto it = dirty_.find(makeKey(cx, cy, cz));
                if (it != dirty_.end())
                    it->second.saving = false;
            }
            for (auto& entry : prepared)
                preparedBlobs_.push_back(std::move(entry));
            FABRIC_LOG_ERROR("ChunkSaveService: batch failed serial={} with unknown error", batchSerial);
        }
    });
}

} // namespace recurse
