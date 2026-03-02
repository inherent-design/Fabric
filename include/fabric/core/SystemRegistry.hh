#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/core/SystemPhase.hh"
#include <array>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace fabric {

struct AppContext;

class SystemRegistry {
  public:
    SystemRegistry() = default;

    /// Register a system for a given phase. Returns reference for chaining.
    /// Throws if a system of the same type is already registered.
    template <typename T, typename... Args> T& registerSystem(SystemPhase phase, Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        registerSystemImpl(phase, std::move(system));
        return ref;
    }

    /// Resolve all dependencies and compute execution order.
    /// Must be called after all registrations, before init.
    /// Returns false if a dependency cycle is detected.
    bool resolve();

    /// Init all systems in resolved order.
    void initAll(AppContext& ctx);

    /// Shutdown all systems in reverse init order.
    void shutdownAll();

    /// Phase dispatch methods. Each calls the appropriate virtual
    /// (update/fixedUpdate/render) on enabled systems in that phase.
    void runPreUpdate(AppContext& ctx, float dt);
    void runFixedUpdate(AppContext& ctx, float fixedDt);
    void runUpdate(AppContext& ctx, float dt);
    void runPostUpdate(AppContext& ctx, float dt);
    void runPreRender(AppContext& ctx);
    void runRender(AppContext& ctx);
    void runPostRender(AppContext& ctx);

    /// Runtime enable/disable (skips system during dispatch).
    template <typename T> void setEnabled(bool enabled) { setEnabledImpl(std::type_index(typeid(T)), enabled); }

    template <typename T> bool isEnabled() const { return isEnabledImpl(std::type_index(typeid(T))); }

    /// Get a registered system by type. Returns nullptr if not found.
    template <typename T> T* get() const {
        auto* base = getImpl(std::type_index(typeid(T)));
        return static_cast<T*>(base);
    }

    /// Debug info for all registered systems.
    struct SystemInfo {
        std::string name;
        SystemPhase phase;
        bool enabled;
        std::type_index typeId;
    };
    std::vector<SystemInfo> listSystems() const;

    SystemRegistry(const SystemRegistry&) = delete;
    SystemRegistry& operator=(const SystemRegistry&) = delete;

  private:
    struct SystemEntry {
        std::unique_ptr<SystemBase> system;
        SystemPhase phase;
        bool enabled = true;
    };

    void registerSystemImpl(SystemPhase phase, std::unique_ptr<SystemBase> system);
    void setEnabledImpl(std::type_index id, bool enabled);
    bool isEnabledImpl(std::type_index id) const;
    SystemBase* getImpl(std::type_index id) const;

    std::unordered_map<std::type_index, SystemEntry> systems_;
    std::vector<std::type_index> initOrder_;
    std::array<std::vector<std::type_index>, kSystemPhaseCount> phaseOrder_;
    bool resolved_ = false;
};

} // namespace fabric
