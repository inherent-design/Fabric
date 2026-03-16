#pragma once

#include <cassert>
#include <chrono>
#include <future>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fabric::platform {

/// RAII task group that manages a set of keyed futures.
/// Entries are submitted with a key and polled by budget. The destructor
/// waits on all outstanding futures, guaranteeing no dangling async work.
/// Does not own a thread; purely manages futures produced elsewhere.
template <typename Key, typename Result, typename Hash = std::hash<Key>, typename Metadata = void>
class ScopedTaskGroup {
  public:
    struct CompletedEntry {
        Key key;
        Result result;
        bool cancelled;
        Metadata metadata;
    };

    ScopedTaskGroup() = default;
    ~ScopedTaskGroup() { drain(); }

    ScopedTaskGroup(ScopedTaskGroup&& other) noexcept : entries_(std::move(other.entries_)) {}

    ScopedTaskGroup& operator=(ScopedTaskGroup&& other) noexcept {
        if (this != &other) {
            drain();
            entries_ = std::move(other.entries_);
        }
        return *this;
    }

    ScopedTaskGroup(const ScopedTaskGroup&) = delete;
    ScopedTaskGroup& operator=(const ScopedTaskGroup&) = delete;

    /// Insert a future with the given key and metadata. Returns false if the key already exists.
    bool submit(Key key, std::future<Result> future, Metadata meta) {
        auto [_, inserted] = entries_.try_emplace(std::move(key), Entry{std::move(future), false, std::move(meta)});
        return inserted;
    }

    /// Mark an entry as cancelled. Returns true if the key was found.
    bool cancel(const Key& key) {
        auto it = entries_.find(key);
        if (it == entries_.end())
            return false;
        it->second.cancelled = true;
        return true;
    }

    /// Mark all entries as cancelled.
    void cancelAll() {
        for (auto& [_, entry] : entries_)
            entry.cancelled = true;
    }

    /// Check if a key exists in the group.
    bool has(const Key& key) const { return entries_.contains(key); }

    /// Poll up to budget ready entries, removing them from the group.
    std::vector<CompletedEntry> poll(int budget) {
        std::vector<CompletedEntry> completed;
        int count = 0;

        for (auto it = entries_.begin(); it != entries_.end() && count < budget;) {
            if (it->second.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                ++it;
                continue;
            }

            completed.push_back(CompletedEntry{
                it->first,
                it->second.future.get(),
                it->second.cancelled,
                std::move(it->second.metadata),
            });
            it = entries_.erase(it);
            ++count;
        }

        return completed;
    }

    /// Number of entries currently tracked.
    size_t size() const { return entries_.size(); }

    /// True if no entries are tracked.
    bool empty() const { return entries_.empty(); }

  private:
    struct Entry {
        std::future<Result> future;
        bool cancelled = false;
        Metadata metadata;
    };

    void drain() {
        for (auto& [_, entry] : entries_)
            entry.future.wait();
        entries_.clear();
    }

    std::unordered_map<Key, Entry, Hash> entries_;
};

/// Specialization for Metadata = void (no per-entry metadata).
template <typename Key, typename Result, typename Hash> class ScopedTaskGroup<Key, Result, Hash, void> {
  public:
    struct CompletedEntry {
        Key key;
        Result result;
        bool cancelled;
    };

    ScopedTaskGroup() = default;
    ~ScopedTaskGroup() { drain(); }

    ScopedTaskGroup(ScopedTaskGroup&& other) noexcept : entries_(std::move(other.entries_)) {}

    ScopedTaskGroup& operator=(ScopedTaskGroup&& other) noexcept {
        if (this != &other) {
            drain();
            entries_ = std::move(other.entries_);
        }
        return *this;
    }

    ScopedTaskGroup(const ScopedTaskGroup&) = delete;
    ScopedTaskGroup& operator=(const ScopedTaskGroup&) = delete;

    /// Insert a future with the given key. Returns false if the key already exists.
    bool submit(Key key, std::future<Result> future) {
        auto [_, inserted] = entries_.try_emplace(std::move(key), Entry{std::move(future), false});
        return inserted;
    }

    /// Mark an entry as cancelled. Returns true if the key was found.
    bool cancel(const Key& key) {
        auto it = entries_.find(key);
        if (it == entries_.end())
            return false;
        it->second.cancelled = true;
        return true;
    }

    /// Mark all entries as cancelled.
    void cancelAll() {
        for (auto& [_, entry] : entries_)
            entry.cancelled = true;
    }

    /// Check if a key exists in the group.
    bool has(const Key& key) const { return entries_.contains(key); }

    /// Poll up to budget ready entries, removing them from the group.
    std::vector<CompletedEntry> poll(int budget) {
        std::vector<CompletedEntry> completed;
        int count = 0;

        for (auto it = entries_.begin(); it != entries_.end() && count < budget;) {
            if (it->second.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                ++it;
                continue;
            }

            completed.push_back(CompletedEntry{
                it->first,
                it->second.future.get(),
                it->second.cancelled,
            });
            it = entries_.erase(it);
            ++count;
        }

        return completed;
    }

    /// Number of entries currently tracked.
    size_t size() const { return entries_.size(); }

    /// True if no entries are tracked.
    bool empty() const { return entries_.empty(); }

  private:
    struct Entry {
        std::future<Result> future;
        bool cancelled = false;
    };

    void drain() {
        for (auto& [_, entry] : entries_)
            entry.future.wait();
        entries_.clear();
    }

    std::unordered_map<Key, Entry, Hash> entries_;
};

} // namespace fabric::platform
