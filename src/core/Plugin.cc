#include "fabric/core/Plugin.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/core/Log.hh"
#include <algorithm>
#include <vector>

namespace fabric {

PluginManager& PluginManager::getInstance() {
  static PluginManager instance;
  return instance;
}

void PluginManager::registerPlugin(const std::string& name, const PluginFactory& factory) {
  std::lock_guard<std::mutex> lock(pluginMutex);
  
  if (name.empty()) {
    throwError("Plugin name cannot be empty");
  }
  
  if (!factory) {
    throwError("Plugin factory cannot be null");
  }
  
  if (pluginFactories.find(name) != pluginFactories.end()) {
    throwError("Plugin '" + name + "' is already registered");
  }
  
  pluginFactories[name] = factory;
  FABRIC_LOG_DEBUG("Registered plugin '{}'", name);
}

bool PluginManager::loadPlugin(const std::string& name) {
  std::lock_guard<std::mutex> lock(pluginMutex);
  
  // Check if already loaded
  if (loadedPlugins.find(name) != loadedPlugins.end()) {
    FABRIC_LOG_WARN("Plugin '{}' is already loaded", name);
    return true;
  }
  
  // Find factory
  auto factoryIt = pluginFactories.find(name);
  if (factoryIt == pluginFactories.end()) {
    FABRIC_LOG_ERROR("Plugin '{}' is not registered", name);
    return false;
  }
  
  try {
    // Create plugin instance
    auto plugin = factoryIt->second();
    if (!plugin) {
      FABRIC_LOG_ERROR("Failed to create plugin '{}'", name);
      return false;
    }
    
    // Add to loaded plugins
    loadedPlugins[name] = plugin;
    FABRIC_LOG_INFO("Loaded plugin '{}' ({}) by {}", name,
                    plugin->getVersion(), plugin->getAuthor());
    
    return true;
  } catch (const std::exception& e) {
    FABRIC_LOG_ERROR("Exception loading plugin '{}': {}", name, e.what());
    return false;
  } catch (...) {
    FABRIC_LOG_ERROR("Unknown exception loading plugin '{}'", name);
    return false;
  }
}

bool PluginManager::unloadPlugin(const std::string& name) {
  std::shared_ptr<Plugin> pluginToUnload;
  
  {
    std::lock_guard<std::mutex> lock(pluginMutex);
    
    auto it = loadedPlugins.find(name);
    if (it == loadedPlugins.end()) {
      FABRIC_LOG_WARN("Plugin '{}' is not loaded", name);
      return false;
    }
    
    // Store the plugin to unload outside the lock
    pluginToUnload = it->second;
    
    // Remove from loaded plugins immediately to prevent cyclic dependencies
    loadedPlugins.erase(it);
  }
  
  try {
    // Shut down the plugin outside the lock to prevent deadlocks
    if (pluginToUnload) {
      pluginToUnload->shutdown();
    }
    
    FABRIC_LOG_INFO("Unloaded plugin '{}'", name);
    return true;
  } catch (const std::exception& e) {
    FABRIC_LOG_ERROR("Exception unloading plugin '{}': {}", name, e.what());
    return false;
  } catch (...) {
    FABRIC_LOG_ERROR("Unknown exception unloading plugin '{}'", name);
    return false;
  }
}

std::shared_ptr<Plugin> PluginManager::getPlugin(const std::string& name) const {
  std::lock_guard<std::mutex> lock(pluginMutex);
  
  auto it = loadedPlugins.find(name);
  if (it == loadedPlugins.end()) {
    return nullptr;
  }
  
  return it->second;
}

std::unordered_map<std::string, std::shared_ptr<Plugin>> PluginManager::getPlugins() const {
  std::lock_guard<std::mutex> lock(pluginMutex);
  return loadedPlugins; // Return a copy for thread safety
}

bool PluginManager::initializeAll() {
  // Create a copy of the plugins to avoid holding the lock during initialization
  std::vector<std::pair<std::string, std::shared_ptr<Plugin>>> plugins;
  
  {
    std::lock_guard<std::mutex> lock(pluginMutex);
    plugins.reserve(loadedPlugins.size());
    for (const auto& pair : loadedPlugins) {
      plugins.push_back(pair);
    }
  }
  
  bool success = true;
  
  for (const auto& [name, plugin] : plugins) {
    if (!plugin) {
      FABRIC_LOG_ERROR("Null plugin reference for '{}'", name);
      success = false;
      continue;
    }
    
    try {
      if (!plugin->initialize()) {
        FABRIC_LOG_ERROR("Failed to initialize plugin '{}'", name);
        success = false;
      } else {
        FABRIC_LOG_INFO("Initialized plugin '{}'", name);
      }
    } catch (const std::exception& e) {
      FABRIC_LOG_ERROR("Exception initializing plugin '{}': {}", name, e.what());
      success = false;
    } catch (...) {
      FABRIC_LOG_ERROR("Unknown exception initializing plugin '{}'", name);
      success = false;
    }
  }
  
  return success;
}

void PluginManager::shutdownAll() {
  // Copy all plugins to a vector for shutdown
  // This allows us to control shutdown order and handle dependencies
  std::vector<std::pair<std::string, std::shared_ptr<Plugin>>> plugins;
  
  {
    std::lock_guard<std::mutex> lock(pluginMutex);
    plugins.reserve(loadedPlugins.size());
    for (const auto& pair : loadedPlugins) {
      plugins.push_back(pair);
    }
    
    // Clear the loaded plugins container first to prevent cyclical shutdown dependencies
    loadedPlugins.clear();
  }
  
  // NOTE: This reversal does not guarantee dependency-correct shutdown order.
  // Plugins are stored in an unordered_map, so iteration order (and therefore
  // reversal order) is implementation-defined and unrelated to load sequence.
  // Proper dependency-ordered shutdown requires an explicit dependency graph.
  std::reverse(plugins.begin(), plugins.end());
  
  for (const auto& [name, plugin] : plugins) {
    if (!plugin) {
      FABRIC_LOG_WARN("Null plugin reference for '{}' during shutdown", name);
      continue;
    }
    
    try {
      plugin->shutdown();
      FABRIC_LOG_INFO("Shut down plugin '{}'", name);
    } catch (const std::exception& e) {
      FABRIC_LOG_ERROR("Exception shutting down plugin '{}': {}", name, e.what());
    } catch (...) {
      FABRIC_LOG_ERROR("Unknown exception shutting down plugin '{}'", name);
    }
  }
}

} // namespace fabric