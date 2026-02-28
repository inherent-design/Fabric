#pragma once

#include "fabric/core/Component.hh"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fabric {

class Plugin {
  public:
    virtual ~Plugin() = default;

    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getAuthor() const = 0;
    virtual std::string getDescription() const = 0;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual std::vector<std::shared_ptr<Component>> getComponents() = 0;

    virtual std::vector<std::string> getDependencies() const { return {}; }

    virtual std::optional<std::string> getLibraryPath() const { return libraryPath_; }

    void setLibraryPath(const std::string& path) { libraryPath_ = path; }

  protected:
    std::optional<std::string> libraryPath_;
};

using PluginFactory = std::function<std::shared_ptr<Plugin>()>;

class PluginManager {
  public:
    PluginManager() = default;

    void registerPlugin(const std::string& name, const PluginFactory& factory);
    bool loadPlugin(const std::string& name);
    bool unloadPlugin(const std::string& name);
    bool reloadPlugin(const std::string& name);

    std::shared_ptr<Plugin> getPlugin(const std::string& name) const;
    std::unordered_map<std::string, std::shared_ptr<Plugin>> getPlugins() const;

    bool initializeAll();
    void shutdownAll();

    bool hasDependencyCycle() const;
    std::vector<std::string> getInitializationOrder() const;

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

  private:
    struct PluginInfo {
        PluginFactory factory;
        std::vector<std::string> dependencies;
        std::shared_ptr<Plugin> instance;
        std::optional<std::string> libraryPath;
        bool isLoaded = false;
    };

    bool validateDependencies(const std::string& name) const;
    bool detectCycle(const std::string& start, std::unordered_set<std::string>& visited,
                     std::unordered_set<std::string>& recursionStack) const;
    std::vector<std::string> computeTopologicalOrder() const;

    mutable std::mutex pluginMutex;
    std::unordered_map<std::string, PluginInfo> plugins;
};

#define FABRIC_REGISTER_PLUGIN(manager, PluginClass)                                                                   \
    (manager).registerPlugin(#PluginClass,                                                                             \
                             []() -> std::shared_ptr<fabric::Plugin> { return std::make_shared<PluginClass>(); })

} // namespace fabric
