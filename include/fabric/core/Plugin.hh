#pragma once

#include "fabric/core/Component.hh"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fabric {

/**
 * @brief Interface for plugins in the Fabric framework
 *
 * Plugins extend the functionality of the Fabric framework by providing
 * additional components, services, or functionality.
 */
class Plugin {
  public:
    /**
     * @brief Virtual destructor
     */
    virtual ~Plugin() = default;

    /**
     * @brief Get the plugin name
     *
     * @return Plugin name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get the plugin version
     *
     * @return Plugin version
     */
    virtual std::string getVersion() const = 0;

    /**
     * @brief Get the plugin author
     *
     * @return Plugin author
     */
    virtual std::string getAuthor() const = 0;

    /**
     * @brief Get the plugin description
     *
     * @return Plugin description
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Initialize the plugin
     *
     * @return true if initialization succeeded, false otherwise
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shut down the plugin
     */
    virtual void shutdown() = 0;

    /**
     * @brief Get the components provided by this plugin
     *
     * @return Vector of components
     */
    virtual std::vector<std::shared_ptr<Component>> getComponents() = 0;
};

/**
 * @brief Plugin factory function type
 */
using PluginFactory = std::function<std::shared_ptr<Plugin>()>;

/**
 * @brief Manages plugins in the Fabric framework
 *
 * The PluginManager keeps track of loaded plugins and provides
 * methods for loading, unloading, and accessing plugins.
 *
 * Thread safety: All methods are thread-safe.
 */
class PluginManager {
  public:
    PluginManager() = default;

    /**
     * @brief Register a plugin factory
     *
     * @param name Plugin name
     * @param factory Plugin factory function
     * @throws FabricException if name is empty, factory is null, or plugin is already registered
     */
    void registerPlugin(const std::string& name, const PluginFactory& factory);

    /**
     * @brief Load a plugin by name
     *
     * @param name Plugin name
     * @return true if the plugin was loaded, false otherwise
     */
    bool loadPlugin(const std::string& name);

    /**
     * @brief Unload a plugin by name
     *
     * @param name Plugin name
     * @return true if the plugin was unloaded, false otherwise
     */
    bool unloadPlugin(const std::string& name);

    /**
     * @brief Get a plugin by name
     *
     * @param name Plugin name
     * @return Plugin or nullptr if not found
     */
    std::shared_ptr<Plugin> getPlugin(const std::string& name) const;

    /**
     * @brief Get all loaded plugins
     *
     * @return Map of plugin names to plugins
     * @note This returns a copy to ensure thread safety
     */
    std::unordered_map<std::string, std::shared_ptr<Plugin>> getPlugins() const;

    /**
     * @brief Initialize all loaded plugins
     *
     * @return true if all plugins initialized successfully, false otherwise
     */
    bool initializeAll();

    /**
     * @brief Shut down all loaded plugins
     *
     * This method shuts down plugins in reverse dependency order
     * to ensure proper cleanup.
     */
    void shutdownAll();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

  private:
    mutable std::mutex pluginMutex;
    std::unordered_map<std::string, PluginFactory> pluginFactories;
    std::unordered_map<std::string, std::shared_ptr<Plugin>> loadedPlugins;
};

/**
 * @brief Helper macro for plugin registration
 *
 * Registers a plugin factory with a PluginManager instance.
 *
 * @param manager PluginManager reference
 * @param PluginClass Plugin class name
 */
#define FABRIC_REGISTER_PLUGIN(manager, PluginClass)                                                                   \
    (manager).registerPlugin(#PluginClass,                                                                             \
                             []() -> std::shared_ptr<fabric::Plugin> { return std::make_shared<PluginClass>(); })

} // namespace fabric
