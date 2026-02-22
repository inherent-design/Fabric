#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace fabric {

/**
 * @brief State of a resource in the resource management system
 */
enum class ResourceState {
  Unloaded,      // Resource is not loaded
  Loading,       // Resource is currently being loaded
  Loaded,        // Resource is fully loaded and ready to use
  LoadingFailed, // Resource failed to load
  Unloading      // Resource is being unloaded
};

/**
 * @brief Priority of a resource load operation
 */
enum class ResourcePriority {
  Lowest,  // Background loading, lowest priority
  Low,     // Lower than normal priority
  Normal,  // Default priority for most resources
  High,    // Higher than normal priority
  Highest  // Critical resources, highest priority
};

/**
 * @brief Base class for all resource types
 * 
 * Resources are assets that can be loaded, unloaded, and managed
 * by the resource management system.
 */
class Resource {
public:
  /**
   * @brief Constructor
   * 
   * @param id Unique identifier for this resource
   */
  explicit Resource(std::string id)
    : id_(std::move(id)), state_(ResourceState::Unloaded) {}
  
  /**
   * @brief Virtual destructor
   */
  virtual ~Resource() = default;
  
  /**
   * @brief Get the resource ID
   * 
   * @return Resource ID
   */
  const std::string& getId() const { return id_; }
  
  /**
   * @brief Get the current state of the resource
   * 
   * @return Resource state
   */
  ResourceState getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
  }
  
  /**
   * @brief Get the current load count of the resource
   * 
   * @return The number of times the resource has been loaded without being unloaded
   */
  int getLoadCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadCount_;
  }
  
  /**
   * @brief Get the estimated memory usage of the resource in bytes
   * 
   * @return Memory usage in bytes
   */
  virtual size_t getMemoryUsage() const = 0;
  
  /**
   * @brief Load the resource
   * 
   * This method loads the resource synchronously.
   * 
   * @return true if the resource was loaded successfully
   */
  bool load() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ == ResourceState::Loaded) {
        // Resource is already loaded, just increment the load count
        loadCount_++;
        return true;
      }
      state_ = ResourceState::Loading;
    }
    
    bool success = loadImpl();
    
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (success) {
        state_ = ResourceState::Loaded;
        loadCount_++;
      } else {
        state_ = ResourceState::LoadingFailed;
      }
    }
    
    return success;
  }
  
  /**
   * @brief Unload the resource
   * 
   * This method unloads the resource, freeing associated memory.
   */
  void unload() {
    bool shouldUnload = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ == ResourceState::Unloaded) {
        return;
      }
      
      // Decrement load count, only actually unload when it reaches 0
      if (loadCount_ > 0) {
        loadCount_--;
      }
      
      if (loadCount_ == 0) {
        state_ = ResourceState::Unloading;
        shouldUnload = true;
      }
    }
    
    // Only call unloadImpl if we're actually unloading
    if (shouldUnload) {
      unloadImpl();
      
      std::lock_guard<std::mutex> lock(mutex_);
      state_ = ResourceState::Unloaded;
    }
  }
  
protected:
  /**
   * @brief Implementation of the resource loading logic
   * 
   * @return true if loading succeeded
   */
  virtual bool loadImpl() = 0;
  
  /**
   * @brief Implementation of the resource unloading logic
   */
  virtual void unloadImpl() = 0;
  
private:
  std::string id_;
  ResourceState state_;
  mutable std::mutex mutex_;
  int loadCount_ = 0; // Track how many times load() has been called without unload()
};

/**
 * @brief Factory for creating resources of different types
 */
class ResourceFactory {
public:
  /**
   * @brief Register a factory function for a resource type
   * 
   * @tparam T Resource type
   * @param typeId Type identifier
   * @param factory Factory function
   */
  template <typename T>
  static void registerType(const std::string& typeId, std::function<std::shared_ptr<T>(const std::string&)> factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    factories_[typeId] = [factory](const std::string& id) {
      return std::static_pointer_cast<Resource>(factory(id));
    };
  }
  
  /**
   * @brief Create a resource of the specified type
   * 
   * @param typeId Type identifier
   * @param id Resource identifier
   * @return Shared pointer to the created resource, or nullptr if the type is not registered
   */
  static std::shared_ptr<Resource> create(const std::string& typeId, const std::string& id);
  
  /**
   * @brief Check if a resource type is registered
   * 
   * @param typeId Type identifier
   * @return true if the type is registered
   */
  static bool isTypeRegistered(const std::string& typeId);
  
private:
  static std::mutex mutex_;
  static std::unordered_map<std::string, std::function<std::shared_ptr<Resource>(const std::string&)>> factories_;
};

/**
 * @brief A reference-counted handle to a resource
 * 
 * ResourceHandle provides safe access to resources managed by the ResourceHub.
 * It automatically maintains reference counting and ensures resources are loaded when needed.
 * 
 * @tparam T The resource type
 */
template <typename T>
class ResourceHandle {
public:
  /**
   * @brief Default constructor - creates an empty handle
   */
  ResourceHandle() = default;
  
  /**
   * @brief Construct from a resource pointer
   *
   * @param resource Pointer to the resource
   */
  explicit ResourceHandle(std::shared_ptr<T> resource)
    : resource_(std::move(resource)) {}
  
  /**
   * @brief Get the resource pointer
   * 
   * @return Pointer to the resource, or nullptr if the handle is empty
   */
  T* get() const {
    return resource_.get();
  }
  
  /**
   * @brief Access the resource via arrow operator
   * 
   * @return Pointer to the resource
   */
  T* operator->() const {
    return get();
  }
  
  /**
   * @brief Check if the handle contains a valid resource
   * 
   * @return true if the handle is not empty
   */
  explicit operator bool() const {
    return resource_ != nullptr;
  }
  
  /**
   * @brief Get the resource ID
   * 
   * @return Resource ID, or empty string if the handle is empty
   */
  std::string getId() const {
    return resource_ ? resource_->getId() : "";
  }
  
  /**
   * @brief Reset the resource handle, releasing the reference
   */
  void reset() {
    resource_.reset();
  }

private:
  std::shared_ptr<T> resource_;
};

/**
 * @brief Load request for the resource manager
 */
struct ResourceLoadRequest {
  std::string typeId;
  std::string resourceId;
  ResourcePriority priority;
  std::function<void(std::shared_ptr<Resource>)> callback;
};

/**
 * @brief Comparator for prioritizing load requests
 */
struct ResourceLoadRequestComparator {
  bool operator()(const ResourceLoadRequest& a, const ResourceLoadRequest& b) const {
    return static_cast<int>(a.priority) < static_cast<int>(b.priority);
  }
};

} // namespace fabric