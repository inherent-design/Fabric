#include "fabric/resource/ResourceHub.hh"
#include "fabric/log/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <algorithm>
#include <thread>

namespace fabric {

void ResourceHub::enforceBudget() {
    enforceMemoryBudget();
}

ResourceHub::~ResourceHub() {
    try {
        shutdown();
    } catch (...) {
        FABRIC_LOG_ERROR("Exception in ResourceHub destructor");
    }
}

void ResourceHub::shutdown() {
    if (shutdown_)
        return;
    shutdown_ = true;
    scheduler_.reset();
    try {
        clear();
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("ResourceHub::shutdown: clear() failed: {}", e.what());
    }
}

// Implementation of other non-template methods
bool ResourceHub::addDependency(const std::string& dependentId, const std::string& dependencyId) {
    try {
        return resourceGraph_.addEdge(dependentId, dependencyId);
    } catch (const CycleDetectedException& e) {
        FABRIC_LOG_WARN("ResourceHub: cycle detected adding dependency {} -> {}: {}", dependentId, dependencyId,
                        e.what());
        return false;
    }
}

bool ResourceHub::removeDependency(const std::string& dependentId, const std::string& dependencyId) {
    return resourceGraph_.removeEdge(dependentId, dependencyId);
}

bool ResourceHub::unload(const std::string& resourceId, bool cascade) {
    if (cascade) {
        // Unload in dependency order
        return unloadRecursive(resourceId);
    } else {
        auto resourceNode = resourceGraph_.getNode(resourceId);
        if (!resourceNode) {
            // Resource not found
            return false;
        }

        auto nodeLock =
            resourceNode->tryLock(CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::NodeModify, true);
        if (!nodeLock || !nodeLock->isLocked()) {
            return false;
        }

        auto resource = nodeLock->getNode()->getDataNoLock();

        // Check if there are dependencies on this resource
        auto dependents = resourceGraph_.getInEdges(resourceId);
        if (!dependents.empty()) {
            // Can't unload if other resources depend on this one
            return false;
        }

        // Unload the resource
        ResourceState state = resource->getState();
        if (state == ResourceState::Loaded) {
            resource->unload();
        }

        // Release the lock before removing the node to avoid deadlocks
        nodeLock->release();

        // Remove from graph
        return resourceGraph_.removeNode(resourceId);
    }
}

bool ResourceHub::unload(const std::string& resourceId) {
    return unload(resourceId, false);
}

bool ResourceHub::unloadRecursive(const std::string& resourceId) {
    // Get dependencies in topological order to ensure proper unloading
    std::vector<std::string> unloadOrder;

    // Helper function for DFS traversal
    std::function<void(const std::string&, std::unordered_set<std::string>&)> collectDependents;
    collectDependents = [&](const std::string& id, std::unordered_set<std::string>& visited) {
        visited.insert(id);

        // First recurse to all dependents
        auto dependents = resourceGraph_.getInEdges(id);
        for (const auto& dependent : dependents) {
            if (visited.find(dependent) == visited.end()) {
                collectDependents(dependent, visited);
            }
        }

        // Then add this resource
        unloadOrder.push_back(id);
    };

    // Collect dependents of this resource
    std::unordered_set<std::string> visited;
    collectDependents(resourceId, visited);

    // Unload in topological order
    bool success = true;
    for (const auto& id : unloadOrder) {
        auto node = resourceGraph_.getNode(id);
        if (node) {
            auto nodeLock = resourceGraph_.tryLockNode(
                id, CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::NodeModify, true);
            if (nodeLock && nodeLock->isLocked()) {
                auto res = nodeLock->getNode()->getDataNoLock();
                if (res->getState() == ResourceState::Loaded) {
                    res->unload();
                }
                nodeLock->release();
                success &= resourceGraph_.removeNode(id);
            }
        }
    }

    return success;
}

void ResourceHub::preload(const std::vector<std::string>& typeIds, const std::vector<std::string>& resourceIds,
                          ResourcePriority priority) {
    if (typeIds.size() != resourceIds.size()) {
        throwError("typeIds and resourceIds must have the same size");
    }

    for (size_t i = 0; i < resourceIds.size(); ++i) {
        auto typeId = typeIds[i];
        auto resourceId = resourceIds[i];
        scheduler_->submitBackground([this, typeId, resourceId]() { this->load<Resource>(typeId, resourceId); });
    }
}

void ResourceHub::setMemoryBudget(size_t bytes) {
    memoryBudget_ = bytes;
    // When we set a new budget, check if we need to enforce it
    enforceBudget();
}

size_t ResourceHub::getMemoryUsage() const {
    size_t total = 0;

    // Get all resources from the graph
    auto allResourceIds = resourceGraph_.getAllNodes();

    for (const auto& id : allResourceIds) {
        auto node = resourceGraph_.getNode(id);
        if (!node) {
            continue;
        }

        auto nodeLock = resourceGraph_.tryLockNode(id, CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::Read);
        if (!nodeLock || !nodeLock->isLocked()) {
            continue;
        }

        auto resource = nodeLock->getNode()->getDataNoLock();
        if (resource->getState() == ResourceState::Loaded) {
            total += resource->getMemoryUsage();
        }
    }

    return total;
}

size_t ResourceHub::getMemoryBudget() const {
    return memoryBudget_;
}

bool ResourceHub::isLoaded(const std::string& resourceId) const {
    // Simplified implementation with proper RAII for lock management
    // and cleaner timeout protection
    constexpr int TIMEOUT_MS = 50; // Short timeout for a read-only operation

    try {
        // First check if the node exists
        if (!resourceGraph_.hasNode(resourceId)) {
            return false;
        }

        // Get a shared pointer to the node
        auto node = resourceGraph_.getNode(resourceId, TIMEOUT_MS);
        if (!node) {
            // Node couldn't be retrieved with timeout
            return false;
        }

        // Try to lock the node with a read intent
        auto nodeLock = node->tryLock(CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::Read, TIMEOUT_MS);

        // If the lock couldn't be acquired, the resource isn't accessible
        if (!nodeLock || !nodeLock->isLocked()) {
            return false;
        }

        // Use RAII to ensure the lock is released
        // by creating a scope and releasing at the end
        {
            auto resource = nodeLock->getNode()->getDataNoLock();
            if (!resource) {
                nodeLock->release();
                return false;
            }

            ResourceState state = resource->getState();

            // Release the lock
            nodeLock->release();

            // Return the loaded state
            return state == ResourceState::Loaded;
        }
    } catch (const std::exception& e) {
        // Log error but don't propagate exception
        FABRIC_LOG_ERROR("Exception in isLoaded for {}: {}", resourceId, e.what());
        return false;
    } catch (...) {
        // Catch any other exceptions
        FABRIC_LOG_ERROR("Unknown exception in isLoaded for {}", resourceId);
        return false;
    }
}

std::vector<std::string> ResourceHub::getDependentResources(const std::string& resourceId) const {
    auto dependents = resourceGraph_.getInEdges(resourceId);
    return std::vector<std::string>(dependents.begin(), dependents.end());
}

std::vector<std::string> ResourceHub::getDependencyResources(const std::string& resourceId) const {
    auto dependencies = resourceGraph_.getOutEdges(resourceId);
    return std::vector<std::string>(dependencies.begin(), dependencies.end());
}

std::unordered_set<std::string> ResourceHub::getDependents(const std::string& resourceId) {
    return resourceGraph_.getInEdges(resourceId);
}

std::unordered_set<std::string> ResourceHub::getDependencies(const std::string& resourceId) {
    return resourceGraph_.getOutEdges(resourceId);
}

bool ResourceHub::hasResource(const std::string& resourceId) {
    return resourceGraph_.hasNode(resourceId);
}

size_t ResourceHub::enforceMemoryBudget() {
    // Simplified implementation based on Copy-Then-Process pattern from IMPLEMENTATION_PATTERNS.md

    // Single mutex for budget enforcement with try_lock to prevent contention
    static std::timed_mutex enforceBudgetMutex;

    // Try to acquire the lock without blocking
    if (!enforceBudgetMutex.try_lock()) {
        // Another thread is already enforcing the budget, skip this invocation
        return 0;
    }

    // Use RAII for mutex management
    std::lock_guard<std::timed_mutex> budgetLockGuard(enforceBudgetMutex, std::adopt_lock);

    // Constants for timeout protection
    constexpr int ENFORCE_TIMEOUT_MS = 300; // 300ms total timeout
    constexpr int NODE_TIMEOUT_MS = 25;     // 25ms per node operation timeout

    // Start the timeout timer
    auto startTime = std::chrono::steady_clock::now();

    // Timeout checker function
    auto isTimedOut = [&startTime, ENFORCE_TIMEOUT_MS]() -> bool {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime)
                   .count() > ENFORCE_TIMEOUT_MS;
    };

    // Check if we need to enforce the budget
    size_t currentUsage = 0;
    try {
        currentUsage = getMemoryUsage();

        if (currentUsage <= memoryBudget_) {
            return 0; // No need to enforce budget
        }
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Exception in getMemoryUsage: {}", e.what());
        return 0;
    }

    // Calculate how much memory we need to free
    size_t toFree = currentUsage - memoryBudget_;

    // Get all resource IDs once and copy
    std::vector<std::string> allResourceIds;
    try {
        allResourceIds = resourceGraph_.getAllNodes();
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Failed to get all nodes: {}", e.what());
        return 0;
    }

    // =================================================================
    // Phase 1: Collect candidates (using the Copy-Then-Process pattern)
    // =================================================================

    // Define an eviction candidate structure
    struct EvictionCandidate {
        std::string id;
        std::chrono::steady_clock::time_point lastAccessTime;
        size_t size;
        bool hasDependents;
    };

    std::vector<EvictionCandidate> candidates;
    candidates.reserve(allResourceIds.size());

    // Collect initial candidate information with minimal locking
    for (const auto& id : allResourceIds) {
        if (isTimedOut()) {
            FABRIC_LOG_WARN("enforceMemoryBudget timed out during candidate collection");
            return 0;
        }

        // Don't waste time on nodes that have dependents
        bool hasDependents = false;
        try {
            auto dependents = resourceGraph_.getInEdges(id);
            hasDependents = !dependents.empty();

            if (hasDependents) {
                continue; // Skip resources with dependents
            }
        } catch (const std::exception& e) {
            // Skip if we can't check dependencies
            continue;
        }

        // Get node info with minimal locking
        auto node = resourceGraph_.getNode(id, NODE_TIMEOUT_MS);
        if (!node) {
            continue;
        }

        // Get node data with read lock
        auto nodeLock = node->tryLock(CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::Read, NODE_TIMEOUT_MS);

        if (!nodeLock || !nodeLock->isLocked()) {
            continue;
        }

        // Gather resource information
        std::shared_ptr<Resource> resource;
        size_t resourceSize = 0;
        std::chrono::steady_clock::time_point lastAccess;
        bool isLoaded = false;
        bool hasSingleRef = false;

        try {
            resource = nodeLock->getNode()->getDataNoLock();
            if (resource) {
                resourceSize = resource->getMemoryUsage();
                lastAccess = node->getLastAccessTime();
                isLoaded = resource->getState() == ResourceState::Loaded;
                hasSingleRef = resource.use_count() == 1;
            }
        } catch (const std::exception& e) {
            FABRIC_LOG_DEBUG("ResourceHub: skipping candidate '{}': {}", id, e.what());
        }

        // Release lock immediately
        nodeLock->release();

        // Add to candidates if it meets criteria
        if (resource && isLoaded && hasSingleRef && !hasDependents) {
            candidates.push_back({id, lastAccess, resourceSize, hasDependents});
        }
    }

    // Check if we have any candidates
    if (candidates.empty() || isTimedOut()) {
        return 0;
    }

    // =================================================================
    // Phase 2: Sort candidates by last access time (oldest first)
    // =================================================================
    try {
        std::sort(candidates.begin(), candidates.end(), [](const EvictionCandidate& a, const EvictionCandidate& b) {
            return a.lastAccessTime < b.lastAccessTime;
        });
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Exception sorting candidates: {}", e.what());
        return 0;
    }

    // =================================================================
    // Phase 3: Evict resources until we've freed enough memory
    // =================================================================
    size_t evictedCount = 0;
    size_t freedMemory = 0;

    for (const auto& candidate : candidates) {
        if (isTimedOut()) {
            FABRIC_LOG_WARN("enforceMemoryBudget timed out during eviction");
            break;
        }

        // Double-check dependencies with minimal lock
        try {
            auto dependents = resourceGraph_.getInEdges(candidate.id);
            if (!dependents.empty()) {
                continue; // Skip if it has dependents now
            }
        } catch (const std::exception& e) {
            continue; // Skip if we can't check dependencies
        }

        // Get node with write lock
        auto nodeLock = resourceGraph_.tryLockNode(
            candidate.id, CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::NodeModify, true, NODE_TIMEOUT_MS);

        if (!nodeLock || !nodeLock->isLocked()) {
            continue;
        }

        // Get resource and verify it's still evictable
        std::shared_ptr<Resource> resource;

        try {
            resource = nodeLock->getNode()->getDataNoLock();

            // Double-check conditions under lock
            if (!resource || resource.use_count() > 1 || resource->getState() != ResourceState::Loaded) {
                nodeLock->release();
                continue;
            }

            // Unload the resource
            resource->unload();

            // Update access time
            nodeLock->getNode()->touch();

            // Release the lock
            nodeLock->release();

            // Remove from graph now that it's unloaded
            bool removed = resourceGraph_.removeNode(candidate.id);

            if (removed) {
                // Update stats
                freedMemory += candidate.size;
                evictedCount++;

                // Log success
                FABRIC_LOG_DEBUG("Evicted resource: {}", candidate.id);
            }
        } catch (const std::exception& e) {
            // Make sure lock is released on exception
            try {
                if (nodeLock->isLocked()) {
                    nodeLock->release();
                }
            } catch (...) {
                FABRIC_LOG_WARN("ResourceHub: failed to release lock for '{}'", candidate.id);
            }

            FABRIC_LOG_ERROR("Error evicting resource {}: {}", candidate.id, e.what());
            continue;
        }

        // If we've freed enough memory, we can stop
        if (freedMemory >= toFree) {
            break;
        }
    }

    return evictedCount;
}

void ResourceHub::disableWorkerThreadsForTesting() {
    if (scheduler_)
        scheduler_->disableForTesting();
    workerThreadCount_ = 0;
}

void ResourceHub::restartWorkerThreadsAfterTesting() {
    shutdown_ = false;
    scheduler_ = std::make_unique<JobScheduler>(std::thread::hardware_concurrency());
    workerThreadCount_ = static_cast<unsigned int>(scheduler_->workerCount());
}

unsigned int ResourceHub::getWorkerThreadCount() const {
    return workerThreadCount_;
}

void ResourceHub::setWorkerThreadCount(unsigned int count) {
    if (count == 0)
        throwError("Worker thread count must be at least 1");
    scheduler_ = std::make_unique<JobScheduler>(count);
    workerThreadCount_ = static_cast<unsigned int>(scheduler_->workerCount());
}

ResourceHub::ResourceHub() : memoryBudget_(1024 * 1024 * 1024), workerThreadCount_(0), shutdown_(false) {
    scheduler_ = std::make_unique<JobScheduler>(2);
    workerThreadCount_ = static_cast<unsigned int>(scheduler_->workerCount());
    FABRIC_LOG_DEBUG("ResourceHub initialized with JobScheduler ({} workers)", workerThreadCount_.load());
}

void ResourceHub::clear() {
    // Simplified clear implementation using the Copy-Then-Process pattern
    constexpr int CLEAR_TIMEOUT_MS = 1000; // 1 second timeout
    auto startTime = std::chrono::steady_clock::now();

    // Timeout checker function
    auto isTimedOut = [&startTime, CLEAR_TIMEOUT_MS]() -> bool {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime)
                   .count() > CLEAR_TIMEOUT_MS;
    };

    try {
        // First, get all resource IDs
        std::vector<std::string> allResourceIds;
        try {
            allResourceIds = resourceGraph_.getAllNodes();
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("Failed to get all nodes during clear(): {}", e.what());
            return;
        }

        if (allResourceIds.empty()) {
            return; // Nothing to clear
        }

        // Determine a topological ordering for safe unloading
        std::vector<std::string> orderedIds;
        try {
            orderedIds = resourceGraph_.topologicalSort();
            if (orderedIds.empty() && !allResourceIds.empty()) {
                // Topological sort failed (possibly due to cycles), use the original ID list
                FABRIC_LOG_WARN("Topological sort failed during clear(), using unordered approach");
                orderedIds = allResourceIds;
            }
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("Error in topological sort during clear(): {}", e.what());
            orderedIds = allResourceIds; // Fall back to unordered
        }

        // Process resources in appropriate order
        for (auto it = orderedIds.rbegin(); it != orderedIds.rend(); ++it) {
            const std::string& id = *it;

            // Check for timeout
            if (isTimedOut()) {
                FABRIC_LOG_WARN("clear() timed out during resource unloading");
                break;
            }

            // First, attempt to unload the resource
            try {
                // Get the node
                auto node = resourceGraph_.getNode(id, 50);
                if (!node) {
                    continue;
                }

                // Lock the node to access its data
                auto nodeLock = node->tryLock(CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::NodeModify, 50);

                if (!nodeLock || !nodeLock->isLocked()) {
                    continue;
                }

                auto resource = nodeLock->getNode()->getDataNoLock();
                if (resource && resource->getState() == ResourceState::Loaded) {
                    resource->unload();
                }

                // Release the lock before removing the node
                nodeLock->release();

                // Now remove the node from the graph
                resourceGraph_.removeNode(id);
            } catch (const std::exception& e) {
                FABRIC_LOG_ERROR("Error processing resource {} during clear(): {}", id, e.what());
            }
        }

        // Final check if there are still resources left
        if (!isTimedOut()) {
            try {
                auto remainingIds = resourceGraph_.getAllNodes();
                if (!remainingIds.empty()) {
                    FABRIC_LOG_WARN("Some resources could not be cleared. {} resources remain.", remainingIds.size());
                }
            } catch (const std::exception& e) {
                FABRIC_LOG_ERROR("Error checking remaining resources: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Unexpected exception in clear(): {}", e.what());
    }
}

void ResourceHub::reset() {
    disableWorkerThreadsForTesting();
    clear();
    memoryBudget_ = 1024 * 1024 * 1024;
}

bool ResourceHub::isEmpty() const {
    try {
        return resourceGraph_.empty();
    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Exception in isEmpty(): {}", e.what());
        return false;
    }
}

} // namespace fabric
