#pragma once

// Out-of-class template definitions for CoordinatedGraph resource locking.
// Included by CoordinatedGraphCore.hh after the class definition.

namespace fabric {

// -----------------------------------------------------------------------
// tryLockResource
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
std::unique_ptr<typename CoordinatedGraph<T, KeyType>::ResourceLockHandle>
CoordinatedGraph<T, KeyType>::tryLockResource(const KeyType& resourceKey, LockMode mode, size_t timeoutMs) {
    if (!hasNode(resourceKey)) {
        return nullptr;
    }

    std::thread::id threadId = std::this_thread::get_id();

    if (deadlockDetectionEnabled_) {
        if (wouldCauseDeadlock(resourceKey, threadId)) {
            throw DeadlockDetectedException("Acquiring lock on resource " + std::string(resourceKey) +
                                            " would cause a deadlock");
        }
    }

    LockIntent intent;
    bool forWrite = false;

    switch (mode) {
        case LockMode::Shared:
            intent = LockIntent::Read;
            forWrite = false;
            break;
        case LockMode::Exclusive:
        case LockMode::Upgrade:
            intent = LockIntent::NodeModify;
            forWrite = true;
            break;
    }

    // Lock ordering: lockGraphMutex_ first, then graphMutex_ (via tryLockNode).
    // wouldCauseDeadlock() follows the same ordering.
    {
        std::lock_guard<std::mutex> lock(lockGraphMutex_);

        threadResourceMap_[threadId].insert(resourceKey);

        if (lockHistoryEnabled_) {
            lockHistory_.push_back({"Attempt lock", resourceKey, threadId, std::chrono::steady_clock::now(), mode});
        }

        resourceLockStatus_[resourceKey][threadId] = ResourceLockStatus::Pending;
    }

    auto nodeLock = tryLockNode(resourceKey, intent, forWrite, timeoutMs, nullptr);

    if (!nodeLock || !nodeLock->isLocked()) {
        std::lock_guard<std::mutex> lock(lockGraphMutex_);

        threadResourceMap_[threadId].erase(resourceKey);
        resourceLockStatus_[resourceKey].erase(threadId);

        if (lockHistoryEnabled_) {
            lockHistory_.push_back({"Failed lock", resourceKey, threadId, std::chrono::steady_clock::now(), mode});
        }

        return nullptr;
    }

    ResourceLockStatus status;
    switch (mode) {
        case LockMode::Shared:
            status = ResourceLockStatus::Shared;
            break;
        case LockMode::Exclusive:
            status = ResourceLockStatus::Exclusive;
            break;
        case LockMode::Upgrade:
            status = ResourceLockStatus::Shared;
            break;
    }

    {
        std::lock_guard<std::mutex> lock(lockGraphMutex_);
        resourceNodeLocks_[resourceKey][threadId] = std::move(nodeLock);
        resourceLockStatus_[resourceKey][threadId] = status;

        if (lockHistoryEnabled_) {
            lockHistory_.push_back({"Acquired lock", resourceKey, threadId, std::chrono::steady_clock::now(), mode});
        }
    }

    return std::make_unique<ResourceLockHandle>(this, resourceKey, mode, status, threadId);
}

// -----------------------------------------------------------------------
// releaseResourceLock
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
bool CoordinatedGraph<T, KeyType>::releaseResourceLock(const KeyType& resourceKey, LockMode mode,
                                                       std::thread::id threadId) {
    std::lock_guard<std::mutex> lock(lockGraphMutex_);

    auto threadIt = resourceNodeLocks_.find(resourceKey);
    if (threadIt == resourceNodeLocks_.end()) {
        return false;
    }

    auto lockIt = threadIt->second.find(threadId);
    if (lockIt == threadIt->second.end()) {
        return false;
    }

    lockIt->second.reset();

    threadIt->second.erase(lockIt);
    if (threadIt->second.empty()) {
        resourceNodeLocks_.erase(threadIt);
    }

    threadResourceMap_[threadId].erase(resourceKey);
    if (threadResourceMap_[threadId].empty()) {
        threadResourceMap_.erase(threadId);
    }

    resourceLockStatus_[resourceKey].erase(threadId);
    if (resourceLockStatus_[resourceKey].empty()) {
        resourceLockStatus_.erase(resourceKey);
    }

    if (lockHistoryEnabled_) {
        lockHistory_.push_back({"Released lock", resourceKey, threadId, std::chrono::steady_clock::now(), mode});
    }

    return true;
}

// -----------------------------------------------------------------------
// upgradeResourceLock
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
bool CoordinatedGraph<T, KeyType>::upgradeResourceLock(const KeyType& resourceKey, std::thread::id threadId,
                                                       size_t timeoutMs) {
    {
        std::lock_guard<std::mutex> lock(lockGraphMutex_);

        auto statusIt = resourceLockStatus_.find(resourceKey);
        if (statusIt == resourceLockStatus_.end()) {
            return false;
        }

        auto threadStatusIt = statusIt->second.find(threadId);
        if (threadStatusIt == statusIt->second.end() || threadStatusIt->second != ResourceLockStatus::Shared) {
            return false;
        }

        auto locksIt = resourceNodeLocks_.find(resourceKey);
        if (locksIt == resourceNodeLocks_.end()) {
            return false;
        }

        auto threadLockIt = locksIt->second.find(threadId);
        if (threadLockIt == locksIt->second.end()) {
            return false;
        }

        threadLockIt->second.reset();
    }

    auto nodeLock = tryLockNode(resourceKey, LockIntent::NodeModify, true, timeoutMs, nullptr);

    if (!nodeLock || !nodeLock->isLocked()) {
        auto sharedLock = tryLockNode(resourceKey, LockIntent::Read, false, timeoutMs, nullptr);

        std::lock_guard<std::mutex> lock(lockGraphMutex_);
        if (sharedLock && sharedLock->isLocked()) {
            resourceNodeLocks_[resourceKey][threadId] = std::move(sharedLock);
            resourceLockStatus_[resourceKey][threadId] = ResourceLockStatus::Shared;
        } else {
            releaseResourceLock(resourceKey, LockMode::Upgrade, threadId);
        }

        return false;
    }

    std::lock_guard<std::mutex> lock(lockGraphMutex_);
    resourceNodeLocks_[resourceKey][threadId] = std::move(nodeLock);
    resourceLockStatus_[resourceKey][threadId] = ResourceLockStatus::Exclusive;

    if (lockHistoryEnabled_) {
        lockHistory_.push_back(
            {"Upgraded lock", resourceKey, threadId, std::chrono::steady_clock::now(), LockMode::Exclusive});
    }

    return true;
}

// -----------------------------------------------------------------------
// hasLock / getLockStatus
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
bool CoordinatedGraph<T, KeyType>::hasLock(const KeyType& resourceKey, std::thread::id threadId) const {
    std::lock_guard<std::mutex> lock(lockGraphMutex_);

    auto threadIt = threadResourceMap_.find(threadId);
    if (threadIt == threadResourceMap_.end()) {
        return false;
    }

    return threadIt->second.find(resourceKey) != threadIt->second.end();
}

template <typename T, typename KeyType>
typename CoordinatedGraph<T, KeyType>::ResourceLockStatus
CoordinatedGraph<T, KeyType>::getLockStatus(const KeyType& resourceKey, std::thread::id threadId) const {
    std::lock_guard<std::mutex> lock(lockGraphMutex_);

    auto statusIt = resourceLockStatus_.find(resourceKey);
    if (statusIt == resourceLockStatus_.end()) {
        return ResourceLockStatus::Unlocked;
    }

    auto threadStatusIt = statusIt->second.find(threadId);
    if (threadStatusIt == statusIt->second.end()) {
        return ResourceLockStatus::Unlocked;
    }

    return threadStatusIt->second;
}

// -----------------------------------------------------------------------
// tryLockResourcesInOrder
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
std::vector<std::unique_ptr<typename CoordinatedGraph<T, KeyType>::ResourceLockHandle>>
CoordinatedGraph<T, KeyType>::tryLockResourcesInOrder(const std::vector<KeyType>& resources, LockMode mode,
                                                      size_t timeoutMs) {
    if (resources.empty()) {
        return {};
    }

    auto subgraph = buildResourceLockSubgraph(resources);

    std::vector<KeyType> lockOrder;

    auto topoOrder = getTopologicalOrderForResources(subgraph);
    if (!topoOrder.empty()) {
        lockOrder = std::move(topoOrder);
    } else {
        lockOrder = resources;
        std::sort(lockOrder.begin(), lockOrder.end());
    }

    std::vector<std::unique_ptr<ResourceLockHandle>> lockHandles;
    lockHandles.reserve(lockOrder.size());

    for (const auto& resource : lockOrder) {
        auto lock = tryLockResource(resource, mode, timeoutMs);
        if (!lock || !lock->isLocked()) {
            for (auto& handle : lockHandles) {
                handle->release();
            }
            return {};
        }
        lockHandles.push_back(std::move(lock));
    }

    return lockHandles;
}

// -----------------------------------------------------------------------
// Lock history
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
std::vector<std::tuple<std::string, KeyType, std::thread::id, std::chrono::steady_clock::time_point,
                       typename CoordinatedGraph<T, KeyType>::LockMode>>
CoordinatedGraph<T, KeyType>::getLockHistory() const {
    std::lock_guard<std::mutex> lock(lockGraphMutex_);
    return lockHistory_;
}

template <typename T, typename KeyType> void CoordinatedGraph<T, KeyType>::clearLockHistory() {
    std::lock_guard<std::mutex> lock(lockGraphMutex_);
    lockHistory_.clear();
}

// -----------------------------------------------------------------------
// wouldCauseDeadlock
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
bool CoordinatedGraph<T, KeyType>::wouldCauseDeadlock(const KeyType& resourceKey, std::thread::id threadId) {
    // Lock ordering: lockGraphMutex_ first, then graphMutex_ (shared).
    // This matches tryLockResource() ordering, preventing ABBA deadlock.

    // Step 1: Under lockGraphMutex_, copy the thread's held resources
    std::unordered_set<KeyType> heldResources;
    {
        std::lock_guard<std::mutex> lock(lockGraphMutex_);

        auto threadIt = threadResourceMap_.find(threadId);
        if (threadIt == threadResourceMap_.end() || threadIt->second.empty()) {
            return false;
        }

        heldResources = threadIt->second;

        // Check if another thread holds the resource we want and also
        // holds one of ours (thread-level cycle).
        for (const auto& [otherThreadId, otherResources] : threadResourceMap_) {
            if (otherThreadId == threadId) {
                continue;
            }
            if (otherResources.find(resourceKey) != otherResources.end()) {
                for (const auto& ourResource : heldResources) {
                    if (otherResources.find(ourResource) != otherResources.end()) {
                        return true;
                    }
                }
            }
        }
    }

    // Step 2: Under graphMutex_ (shared), copy outEdges_ for BFS.
    // lockGraphMutex_ is NOT held here -- avoids nested locks entirely.
    std::unordered_map<KeyType, std::unordered_set<KeyType>> localOutEdges;
    {
        auto graphLock = lockGraph(LockIntent::Read);
        if (!graphLock || !graphLock->isLocked()) {
            throw LockAcquisitionException("Failed to acquire graph lock for deadlock detection");
        }
        localOutEdges = outEdges_;
    }

    // Step 3: BFS on local copy (no locks held).
    for (const auto& heldResource : heldResources) {
        std::unordered_set<KeyType> visited;
        std::queue<KeyType> bfsQueue;

        bfsQueue.push(resourceKey);
        visited.insert(resourceKey);

        while (!bfsQueue.empty()) {
            KeyType current = bfsQueue.front();
            bfsQueue.pop();

            if (current == heldResource) {
                return true;
            }

            auto outEdgesIt = localOutEdges.find(current);
            if (outEdgesIt != localOutEdges.end()) {
                for (const auto& nextNode : outEdgesIt->second) {
                    if (visited.insert(nextNode).second) {
                        bfsQueue.push(nextNode);
                    }
                }
            }
        }
    }

    return false;
}

// -----------------------------------------------------------------------
// buildResourceLockSubgraph
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
std::unordered_map<KeyType, std::unordered_set<KeyType>>
CoordinatedGraph<T, KeyType>::buildResourceLockSubgraph(const std::vector<KeyType>& resources) {
    std::unordered_map<KeyType, std::unordered_set<KeyType>> subgraph;

    std::unordered_set<KeyType> resourceSet(resources.begin(), resources.end());

    for (const auto& resource : resources) {
        subgraph[resource] = {};

        auto outEdges = getOutEdges(resource);

        for (const auto& target : outEdges) {
            if (resourceSet.find(target) != resourceSet.end()) {
                subgraph[resource].insert(target);
            }
        }
    }

    return subgraph;
}

// -----------------------------------------------------------------------
// getTopologicalOrderForResources
// -----------------------------------------------------------------------

template <typename T, typename KeyType>
std::vector<KeyType> CoordinatedGraph<T, KeyType>::getTopologicalOrderForResources(
    const std::unordered_map<KeyType, std::unordered_set<KeyType>>& subgraph) {
    std::vector<KeyType> result;
    std::unordered_map<KeyType, bool> visited;
    std::unordered_map<KeyType, bool> inProcess;

    std::function<bool(const KeyType&)> visit = [&](const KeyType& key) {
        if (inProcess[key]) {
            return false;
        }

        if (visited[key]) {
            return true;
        }

        inProcess[key] = true;

        auto it = subgraph.find(key);
        if (it != subgraph.end()) {
            for (const auto& neighbor : it->second) {
                if (!visit(neighbor)) {
                    return false;
                }
            }
        }

        inProcess[key] = false;
        visited[key] = true;
        result.push_back(key);

        return true;
    };

    for (const auto& [key, _] : subgraph) {
        if (!visited[key]) {
            if (!visit(key)) {
                return {};
            }
        }
    }

    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace fabric
