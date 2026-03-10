#pragma once

#include "fabric/core/Log.hh"
#include "fabric/core/Resource.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/CoordinatedGraph.hh"
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace fabric {

// Forward declarations
namespace Test {
class ResourceHubTestHelper;
}

/**
 * @brief Central hub for managing resources with dependency tracking
 *
 * ResourceHub manages loading, unloading, and tracking dependencies between
 * resources using a thread-safe graph structure. It provides both synchronous
 * and asynchronous resource loading options.
 */
class ResourceHub {
    // Allow test helper to access protected members
    friend class fabric::Test::ResourceHubTestHelper;

  public:
    ResourceHub();
    ~ResourceHub();

    /**
     * @brief Load a resource synchronously
     *
     * @tparam T Resource type
     * @param typeId Type identifier
     * @param resourceId Resource identifier
     * @return ResourceHandle for the loaded resource
     */
    template <typename T> ResourceHandle<T> load(const std::string& typeId, const std::string& resourceId) {
        static_assert(std::is_base_of<Resource, T>::value, "T must be derived from Resource");
        constexpr int LOAD_TIMEOUT_MS = 500;
        constexpr int PHASE_TIMEOUT_MS = 150;

        try {
            // Phase 1: Resource lookup or creation
            std::shared_ptr<Resource> resource;

            if (resourceGraph_.hasNode(resourceId)) {
                auto node = resourceGraph_.getNode(resourceId, PHASE_TIMEOUT_MS);
                if (node) {
                    auto nodeLock =
                        node->tryLock(CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::Read, PHASE_TIMEOUT_MS);
                    if (nodeLock && nodeLock->isLocked()) {
                        resource = nodeLock->getNode()->getDataNoLock();
                        nodeLock->release();
                    }
                }
            }

            if (!resource) {
                resource = ResourceFactory::create(typeId, resourceId);
                if (!resource) {
                    FABRIC_LOG_ERROR("Failed to create resource: {}", resourceId);
                    return ResourceHandle<T>();
                }
                if (!resourceGraph_.addNode(resourceId, resource)) {
                    auto node = resourceGraph_.getNode(resourceId, PHASE_TIMEOUT_MS);
                    if (node) {
                        auto nodeLock = node->tryLock(CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::Read,
                                                      PHASE_TIMEOUT_MS);
                        if (nodeLock && nodeLock->isLocked()) {
                            resource = nodeLock->getNode()->getDataNoLock();
                            nodeLock->release();
                        }
                    }
                }
            }

            // Phase 2: Load via scheduler
            if (resource->getState() != ResourceState::Loaded) {
                auto resourceCopy = resource;
                auto future = scheduler_->submit([resourceCopy]() { return resourceCopy->load(); });
                if (future.wait_for(std::chrono::milliseconds(LOAD_TIMEOUT_MS)) == std::future_status::ready) {
                    if (!future.get())
                        FABRIC_LOG_WARN("Failed to load resource: {}", resourceId);
                } else {
                    FABRIC_LOG_WARN("Resource loading timed out for: {}", resourceId);
                }
            }

            return ResourceHandle<T>(std::static_pointer_cast<T>(resource));
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("Exception in ResourceHub::load() for {}: {}", resourceId, e.what());
            return ResourceHandle<T>();
        }
    }

    /**
     * @brief Load a resource asynchronously
     *
     * @tparam T Resource type
     * @param typeId Type identifier
     * @param resourceId Resource identifier
     * @param priority Loading priority
     * @param callback Function to call when the resource is loaded
     */
    template <typename T>
    void loadAsync(const std::string& typeId, const std::string& resourceId, ResourcePriority priority,
                   std::function<void(ResourceHandle<T>)> callback) {
        static_assert(std::is_base_of<Resource, T>::value, "T must be derived from Resource");

        if (resourceGraph_.hasNode(resourceId)) {
            auto node = resourceGraph_.getNode(resourceId);
            if (node) {
                auto nodeLock = node->tryLock(CoordinatedGraph<std::shared_ptr<Resource>>::LockIntent::Read);
                if (nodeLock && nodeLock->isLocked()) {
                    auto resource = nodeLock->getNode()->getDataNoLock();
                    if (resource->getState() == ResourceState::Loaded) {
                        if (callback)
                            callback(ResourceHandle<T>(std::static_pointer_cast<T>(resource)));
                        return;
                    }
                }
            }
        }

        scheduler_->submitBackground([this, typeId, resourceId, callback]() {
            auto handle = this->load<T>(typeId, resourceId);
            if (callback)
                callback(handle);
        });
    }

    /**
     * @brief Add a dependency between two resources
     *
     * @param dependentId ID of the dependent resource
     * @param dependencyId ID of the dependency
     * @return true if dependency was added, false if either resource doesn't
     * exist or dependecy already exists
     */
    bool addDependency(const std::string& dependentId, const std::string& dependencyId);

    /**
     * @brief Remove a dependency between two resources
     *
     * @param dependentId ID of the dependent resource
     * @param dependencyId ID of the dependency
     * @return true if dependency was removed, false if either resource doesn't
     * exist or there was no dependency
     */
    bool removeDependency(const std::string& dependentId, const std::string& dependencyId);

    /**
     * @brief Unload a resource
     *
     * @param resourceId Resource identifier
     * @return true if the resource was unloaded
     */
    bool unload(const std::string& resourceId);

    /**
     * @brief Unload a resource with an option to cascade unload dependencies
     *
     * @param resourceId Resource identifier
     * @param cascade If true, also unload resources that depend on this one
     * @return true if the resource was unloaded
     */
    bool unload(const std::string& resourceId, bool cascade);

    /**
     * @brief Unload a resource and all resources that depend on it
     *
     * @param resourceId Resource identifier
     * @return true if the resource was unloaded
     */
    bool unloadRecursive(const std::string& resourceId);

    /**
     * @brief Preload a batch of resources asynchronously
     *
     * @param typeIds Type identifiers for each resource
     * @param resourceIds Resource identifiers
     * @param priority Loading priority
     */
    void preload(const std::vector<std::string>& typeIds, const std::vector<std::string>& resourceIds,
                 ResourcePriority priority = ResourcePriority::Low);

    /**
     * @brief Set the memory budget for the resource manager
     *
     * @param bytes Memory budget in bytes
     */
    void setMemoryBudget(size_t bytes);

    /**
     * @brief Get the memory budget
     *
     * @return Memory budget in bytes
     */
    size_t getMemoryBudget() const;

    /**
     * @brief Get the current memory usage
     *
     * @return Memory usage in bytes
     */
    size_t getMemoryUsage() const;

    /**
     * @brief Explicitly trigger memory budget enforcement
     *
     * @return The number of resources evicted
     */
    size_t enforceMemoryBudget();

    /**
     * @brief Disable worker threads for testing
     */
    void disableWorkerThreadsForTesting();

    /**
     * @brief Restart worker threads after testing
     */
    void restartWorkerThreadsAfterTesting();

    /**
     * @brief Get the number of worker threads
     *
     * @return Number of worker threads
     */
    unsigned int getWorkerThreadCount() const;

    /**
     * @brief Set the number of worker threads
     *
     * @param count Number of worker threads
     */
    void setWorkerThreadCount(unsigned int count);

    /**
     * @brief Get resources that depend on a specific resource
     *
     * @param resourceId Resource identifier
     * @return Set of resource IDs that depend on the specified resource
     */
    std::unordered_set<std::string> getDependents(const std::string& resourceId);

    /**
     * @brief Get resources that a specific resource depends on
     *
     * @param resourceId Resource identifier
     * @return Set of resource IDs that the specified resource depends on
     */
    std::unordered_set<std::string> getDependencies(const std::string& resourceId);

    /**
     * @brief Check if a resource exists
     *
     * @param resourceId Resource identifier
     * @return true if the resource exists
     */
    bool hasResource(const std::string& resourceId);

    /**
     * @brief Check if a resource is loaded
     *
     * @param resourceId Resource identifier
     * @return true if the resource is loaded
     */
    bool isLoaded(const std::string& resourceId) const;

    /**
     * @brief Get dependent resources as a vector
     *
     * @param resourceId Resource identifier
     * @return Vector of resource IDs that depend on the specified resource
     */
    std::vector<std::string> getDependentResources(const std::string& resourceId) const;

    /**
     * @brief Get dependency resources as a vector
     *
     * @param resourceId Resource identifier
     * @return Vector of resource IDs that the specified resource depends on
     */
    std::vector<std::string> getDependencyResources(const std::string& resourceId) const;

    /**
     * @brief Clear all resources
     *
     * This method unloads and removes all resources from the manager.
     */
    void clear();

    /**
     * @brief Reset the resource hub to a clean state
     *
     * This method is useful for testing. It:
     * 1. Disables worker threads
     * 2. Clears all resources
     * 3. Resets the memory budget to the default value
     */
    void reset();

    /**
     * @brief Check if the resource hub is empty
     *
     * @return true if the hub has no resources
     */
    bool isEmpty() const;

    /**
     * @brief Shutdown the resource manager
     *
     * This method stops all worker threads and unloads all resources.
     * The ResourceHub will no longer be usable after this call.
     */
    void shutdown();

  protected:
    // For testing access - would normally be private but we need it in tests
    CoordinatedGraph<std::shared_ptr<Resource>> resourceGraph_;

  private:
    void enforceBudget();

    std::unique_ptr<JobScheduler> scheduler_;
    std::atomic<size_t> memoryBudget_;
    std::atomic<unsigned int> workerThreadCount_;
    std::atomic<bool> shutdown_{false};
};

} // namespace fabric
