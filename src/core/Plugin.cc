#include "fabric/core/Plugin.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <algorithm>
#include <queue>

namespace fabric {

void PluginManager::registerPlugin(const std::string& name, const PluginFactory& factory) {
    std::lock_guard<std::mutex> lock(pluginMutex);

    if (name.empty()) {
        throwError("Plugin name cannot be empty");
    }

    if (!factory) {
        throwError("Plugin factory cannot be null");
    }

    auto it = plugins.find(name);
    if (it != plugins.end()) {
        throwError("Plugin '" + name + "' is already registered");
    }

    PluginInfo info;
    info.factory = factory;

    try {
        auto prototype = factory();
        if (!prototype) {
            throwError("Plugin factory for '" + name + "' returned null");
        }

        info.dependencies = prototype->getDependencies();
        info.libraryPath = prototype->getLibraryPath();
    } catch (const FabricException&) {
        throw;
    } catch (const std::exception& e) {
        throwError("Failed to inspect plugin '" + name + "': " + std::string(e.what()));
    } catch (...) {
        throwError("Failed to inspect plugin '" + name + "': unknown error");
    }

    plugins.emplace(name, std::move(info));
    FABRIC_LOG_DEBUG("Registered plugin '{}'", name);
}

bool PluginManager::validateDependencies(const std::string& name) const {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;
    return !detectCycle(name, visited, recursionStack);
}

bool PluginManager::detectCycle(const std::string& start, std::unordered_set<std::string>& visited,
                                std::unordered_set<std::string>& recursionStack) const {
    visited.insert(start);
    recursionStack.insert(start);

    auto it = plugins.find(start);
    if (it == plugins.end()) {
        recursionStack.erase(start);
        return false;
    }

    for (const auto& dep : it->second.dependencies) {
        auto depIt = plugins.find(dep);
        if (depIt == plugins.end()) {
            continue;
        }

        if (recursionStack.find(dep) != recursionStack.end()) {
            return true;
        }

        if (visited.find(dep) == visited.end()) {
            if (detectCycle(dep, visited, recursionStack)) {
                return true;
            }
        }
    }

    recursionStack.erase(start);
    return false;
}

std::vector<std::string> PluginManager::computeTopologicalOrder() const {
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adjList;

    for (const auto& pair : plugins) {
        const auto& name = pair.first;
        inDegree[name] = 0;
        adjList[name] = {};
    }

    for (const auto& pair : plugins) {
        const auto& name = pair.first;
        const auto& info = pair.second;

        for (const auto& dep : info.dependencies) {
            auto depIt = plugins.find(dep);
            if (depIt == plugins.end()) {
                continue;
            }

            adjList[dep].push_back(name);
            inDegree[name]++;
        }
    }

    std::queue<std::string> zeroDegree;
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            zeroDegree.push(pair.first);
        }
    }

    std::vector<std::string> result;
    result.reserve(inDegree.size());

    while (!zeroDegree.empty()) {
        std::string current = zeroDegree.front();
        zeroDegree.pop();
        result.push_back(current);

        for (const auto& neighbor : adjList[current]) {
            if (--inDegree[neighbor] == 0) {
                zeroDegree.push(neighbor);
            }
        }
    }

    if (result.size() != inDegree.size()) {
        return {};
    }

    return result;
}

std::vector<std::string> PluginManager::getInitializationOrder() const {
    std::lock_guard<std::mutex> lock(pluginMutex);
    return computeTopologicalOrder();
}

bool PluginManager::hasDependencyCycle() const {
    std::lock_guard<std::mutex> lock(pluginMutex);

    for (const auto& pair : plugins) {
        const auto& name = pair.first;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> recursionStack;
        if (detectCycle(name, visited, recursionStack)) {
            return true;
        }
    }

    return false;
}

bool PluginManager::loadPlugin(const std::string& name) {
    std::lock_guard<std::mutex> lock(pluginMutex);

    auto it = plugins.find(name);
    if (it == plugins.end()) {
        FABRIC_LOG_ERROR("Plugin '{}' is not registered", name);
        return false;
    }

    PluginInfo& info = it->second;
    if (info.isLoaded) {
        FABRIC_LOG_WARN("Plugin '{}' is already loaded", name);
        return true;
    }

    try {
        auto plugin = info.factory();
        if (!plugin) {
            FABRIC_LOG_ERROR("Failed to create plugin '{}'", name);
            return false;
        }

        info.dependencies = plugin->getDependencies();
        auto pluginLibraryPath = plugin->getLibraryPath();
        if (pluginLibraryPath.has_value()) {
            info.libraryPath = pluginLibraryPath;
        }
        if (info.libraryPath.has_value()) {
            plugin->setLibraryPath(*info.libraryPath);
        }

        info.instance = plugin;
        info.isLoaded = true;

        if (!validateDependencies(name)) {
            info.instance.reset();
            info.isLoaded = false;
            FABRIC_LOG_ERROR("Plugin '{}' introduces a dependency cycle", name);
            return false;
        }

        FABRIC_LOG_INFO("Loaded plugin '{}' ({}) by {}", name, plugin->getVersion(), plugin->getAuthor());
        return true;
    } catch (const std::exception& e) {
        info.instance.reset();
        info.isLoaded = false;
        FABRIC_LOG_ERROR("Exception loading plugin '{}': {}", name, e.what());
        return false;
    } catch (...) {
        info.instance.reset();
        info.isLoaded = false;
        FABRIC_LOG_ERROR("Unknown exception loading plugin '{}'", name);
        return false;
    }
}

bool PluginManager::unloadPlugin(const std::string& name) {
    std::shared_ptr<Plugin> pluginToUnload;

    {
        std::lock_guard<std::mutex> lock(pluginMutex);

        auto it = plugins.find(name);
        if (it == plugins.end() || !it->second.isLoaded) {
            FABRIC_LOG_WARN("Plugin '{}' is not loaded", name);
            return false;
        }

        pluginToUnload = it->second.instance;
        it->second.instance.reset();
        it->second.isLoaded = false;
    }

    try {
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

bool PluginManager::reloadPlugin(const std::string& name) {
    {
        std::lock_guard<std::mutex> lock(pluginMutex);
        auto it = plugins.find(name);
        if (it == plugins.end()) {
            FABRIC_LOG_ERROR("Plugin '{}' is not registered", name);
            return false;
        }

        if (!it->second.isLoaded) {
            FABRIC_LOG_WARN("Plugin '{}' is not loaded", name);
            return false;
        }
    }

    if (!unloadPlugin(name)) {
        FABRIC_LOG_WARN("Failed to unload plugin '{}' for reload", name);
        return false;
    }

    return loadPlugin(name);
}

std::shared_ptr<Plugin> PluginManager::getPlugin(const std::string& name) const {
    std::lock_guard<std::mutex> lock(pluginMutex);

    auto it = plugins.find(name);
    if (it == plugins.end() || !it->second.isLoaded) {
        return nullptr;
    }

    return it->second.instance;
}

std::unordered_map<std::string, std::shared_ptr<Plugin>> PluginManager::getPlugins() const {
    std::lock_guard<std::mutex> lock(pluginMutex);

    std::unordered_map<std::string, std::shared_ptr<Plugin>> result;
    for (const auto& pair : plugins) {
        const auto& name = pair.first;
        const auto& info = pair.second;

        if (info.isLoaded && info.instance) {
            result[name] = info.instance;
        }
    }
    return result;
}

bool PluginManager::initializeAll() {
    std::vector<std::pair<std::string, std::shared_ptr<Plugin>>> pluginsToInit;
    std::vector<std::string> order;

    {
        std::lock_guard<std::mutex> lock(pluginMutex);
        order = computeTopologicalOrder();

        if (order.empty() && !plugins.empty()) {
            FABRIC_LOG_ERROR("Cannot initialize plugins due to dependency cycle");
            return false;
        }

        for (const auto& name : order) {
            const auto it = plugins.find(name);
            if (it != plugins.end() && it->second.isLoaded && it->second.instance) {
                pluginsToInit.emplace_back(name, it->second.instance);
            }
        }
    }

    bool success = true;

    for (const auto& pair : pluginsToInit) {
        const auto& name = pair.first;
        const auto& plugin = pair.second;

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
    std::vector<std::pair<std::string, std::shared_ptr<Plugin>>> pluginsToShutdown;
    std::vector<std::string> order;

    {
        std::lock_guard<std::mutex> lock(pluginMutex);
        order = computeTopologicalOrder();

        if (order.empty() && !plugins.empty()) {
            for (const auto& pair : plugins) {
                const auto& name = pair.first;
                const auto& info = pair.second;
                if (info.isLoaded && info.instance) {
                    pluginsToShutdown.emplace_back(name, info.instance);
                }
            }
        } else {
            for (auto it = order.rbegin(); it != order.rend(); ++it) {
                const auto pluginIt = plugins.find(*it);
                if (pluginIt != plugins.end() && pluginIt->second.isLoaded && pluginIt->second.instance) {
                    pluginsToShutdown.emplace_back(*it, pluginIt->second.instance);
                }
            }
        }

        for (auto& pair : plugins) {
            auto& info = pair.second;
            info.instance.reset();
            info.isLoaded = false;
        }
    }

    for (const auto& [name, plugin] : pluginsToShutdown) {
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
