#include "fabric/core/SystemRegistry.hh"
#include "fabric/log/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <queue>

namespace fabric {

void SystemRegistry::registerSystem(SystemPhase phase, std::unique_ptr<SystemBase> system) {
    registerSystemImpl(phase, std::move(system));
}

void SystemRegistry::registerSystemImpl(SystemPhase phase, std::unique_ptr<SystemBase> system) {
    if (resolved_) {
        throwError("Cannot register system '" + system->name() + "' after resolve()");
    }

    auto typeId = system->typeId();
    if (systems_.count(typeId)) {
        throwError("System '" + system->name() + "' is already registered");
    }

    SystemEntry entry;
    entry.system = std::move(system);
    entry.phase = phase;
    systems_.emplace(typeId, std::move(entry));
}

bool SystemRegistry::resolve() {
    // Step 1: Call configureDependencies() on each system
    for (auto& [typeId, entry] : systems_) {
        entry.system->configureDependencies();
    }

    // Step 2: Convert before() edges to after() edges.
    // If A calls before<B>(), that means "A runs before B",
    // equivalent to B having after<A>().
    for (auto& [typeId, entry] : systems_) {
        for (const auto& beforeDep : entry.system->beforeDeps_) {
            auto it = systems_.find(beforeDep);
            if (it != systems_.end()) {
                it->second.system->afterDeps_.push_back(typeId);
            }
        }
    }

    // Step 3: Kahn's algorithm for global init order
    std::unordered_map<std::type_index, int> inDegree;
    std::unordered_map<std::type_index, std::vector<std::type_index>> adjList;

    for (const auto& [typeId, entry] : systems_) {
        inDegree.emplace(typeId, 0);
        adjList[typeId] = {};
    }

    for (const auto& [typeId, entry] : systems_) {
        for (const auto& dep : entry.system->afterDeps_) {
            if (systems_.count(dep)) {
                adjList[dep].push_back(typeId);
                inDegree[typeId]++;
            }
        }
    }

    std::queue<std::type_index> zeroDegree;
    for (const auto& [typeId, degree] : inDegree) {
        if (degree == 0) {
            zeroDegree.push(typeId);
        }
    }

    initOrder_.clear();
    initOrder_.reserve(systems_.size());

    while (!zeroDegree.empty()) {
        auto current = zeroDegree.front();
        zeroDegree.pop();
        initOrder_.push_back(current);

        for (const auto& neighbor : adjList[current]) {
            if (--inDegree[neighbor] == 0) {
                zeroDegree.push(neighbor);
            }
        }
    }

    if (initOrder_.size() != systems_.size()) {
        FABRIC_LOG_ERROR("System dependency cycle detected");
        return false;
    }

    // Step 4: Per-phase ordering (filtered subset of initOrder_)
    for (auto& phaseList : phaseOrder_) {
        phaseList.clear();
    }

    for (const auto& typeId : initOrder_) {
        const auto& entry = systems_.at(typeId);
        auto phaseIdx = static_cast<size_t>(entry.phase);
        phaseOrder_[phaseIdx].push_back(typeId);
    }

    resolved_ = true;
    return true;
}

void SystemRegistry::initAll(AppContext& ctx) {
    size_t initialized = 0;
    try {
        for (const auto& typeId : initOrder_) {
            auto& entry = systems_.at(typeId);
            FABRIC_LOG_DEBUG("Initializing system: {}", entry.system->name());
            entry.system->init(ctx);
            ++initialized;
        }
    } catch (...) {
        FABRIC_LOG_ERROR("System init failed after {} of {} systems, shutting down initialized systems", initialized,
                         initOrder_.size());
        // Shut down successfully initialized systems in reverse order
        for (size_t i = initialized; i > 0; --i) {
            auto entryIt = systems_.find(initOrder_[i - 1]);
            if (entryIt != systems_.end()) {
                FABRIC_LOG_DEBUG("Cleanup shutdown: {}", entryIt->second.system->name());
                entryIt->second.system->shutdown();
            }
        }
        throw;
    }
}

void SystemRegistry::shutdownAll() {
    for (auto it = initOrder_.rbegin(); it != initOrder_.rend(); ++it) {
        auto entryIt = systems_.find(*it);
        if (entryIt != systems_.end()) {
            try {
                FABRIC_LOG_DEBUG("Shutting down system: {}", entryIt->second.system->name());
                entryIt->second.system->shutdown();
            } catch (const std::exception& e) {
                FABRIC_LOG_ERROR("Exception shutting down system {}: {}", entryIt->second.system->name(), e.what());
            } catch (...) {
                FABRIC_LOG_ERROR("Unknown exception shutting down system: {}", entryIt->second.system->name());
            }
        }
    }
}

namespace {
template <typename Systems, typename PhaseOrder, typename Fn>
void dispatchPhase(SystemPhase phase, PhaseOrder& phaseOrder, Systems& systems, Fn&& fn) {
    for (const auto& typeId : phaseOrder[static_cast<size_t>(phase)]) {
        auto& entry = systems.at(typeId);
        if (entry.enabled)
            fn(*entry.system);
    }
}
} // namespace

void SystemRegistry::runPreUpdate(AppContext& ctx, float dt) {
    dispatchPhase(SystemPhase::PreUpdate, phaseOrder_, systems_, [&](SystemBase& s) { s.update(ctx, dt); });
}

void SystemRegistry::runFixedUpdate(AppContext& ctx, float fixedDt) {
    dispatchPhase(SystemPhase::FixedUpdate, phaseOrder_, systems_, [&](SystemBase& s) { s.fixedUpdate(ctx, fixedDt); });
}

void SystemRegistry::runUpdate(AppContext& ctx, float dt) {
    dispatchPhase(SystemPhase::Update, phaseOrder_, systems_, [&](SystemBase& s) { s.update(ctx, dt); });
}

void SystemRegistry::runPostUpdate(AppContext& ctx, float dt) {
    dispatchPhase(SystemPhase::PostUpdate, phaseOrder_, systems_, [&](SystemBase& s) { s.update(ctx, dt); });
}

void SystemRegistry::runPreRender(AppContext& ctx) {
    dispatchPhase(SystemPhase::PreRender, phaseOrder_, systems_, [&](SystemBase& s) { s.render(ctx); });
}

void SystemRegistry::runRender(AppContext& ctx) {
    dispatchPhase(SystemPhase::Render, phaseOrder_, systems_, [&](SystemBase& s) { s.render(ctx); });
}

void SystemRegistry::runPostRender(AppContext& ctx) {
    dispatchPhase(SystemPhase::PostRender, phaseOrder_, systems_, [&](SystemBase& s) { s.render(ctx); });
}

void SystemRegistry::setEnabledImpl(std::type_index id, bool enabled) {
    auto it = systems_.find(id);
    if (it != systems_.end()) {
        it->second.enabled = enabled;
    }
}

bool SystemRegistry::isEnabledImpl(std::type_index id) const {
    auto it = systems_.find(id);
    return it != systems_.end() && it->second.enabled;
}

SystemBase* SystemRegistry::getImpl(std::type_index id) const {
    auto it = systems_.find(id);
    return it != systems_.end() ? it->second.system.get() : nullptr;
}

std::vector<SystemRegistry::SystemInfo> SystemRegistry::listSystems() const {
    std::vector<SystemInfo> result;
    result.reserve(systems_.size());
    for (const auto& [typeId, entry] : systems_) {
        result.push_back({entry.system->name(), entry.phase, entry.enabled, typeId});
    }
    return result;
}

} // namespace fabric
