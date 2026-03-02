#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <queue>

namespace fabric {

void SystemRegistry::registerSystem(SystemPhase phase, std::unique_ptr<SystemBase> system) {
    registerSystemImpl(phase, std::move(system));
}

void SystemRegistry::registerSystemImpl(SystemPhase phase, std::unique_ptr<SystemBase> system) {
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
    for (const auto& typeId : initOrder_) {
        auto& entry = systems_.at(typeId);
        FABRIC_LOG_DEBUG("Initializing system: {}", entry.system->name());
        entry.system->init(ctx);
    }
}

void SystemRegistry::shutdownAll() {
    for (auto it = initOrder_.rbegin(); it != initOrder_.rend(); ++it) {
        auto entryIt = systems_.find(*it);
        if (entryIt != systems_.end()) {
            FABRIC_LOG_DEBUG("Shutting down system: {}", entryIt->second.system->name());
            entryIt->second.system->shutdown();
        }
    }
}

// Phase dispatch: PreUpdate, Update, PostUpdate call update(ctx, dt)
void SystemRegistry::runPreUpdate(AppContext& ctx, float dt) {
    for (const auto& typeId : phaseOrder_[static_cast<size_t>(SystemPhase::PreUpdate)]) {
        auto& entry = systems_.at(typeId);
        if (entry.enabled)
            entry.system->update(ctx, dt);
    }
}

void SystemRegistry::runFixedUpdate(AppContext& ctx, float fixedDt) {
    for (const auto& typeId : phaseOrder_[static_cast<size_t>(SystemPhase::FixedUpdate)]) {
        auto& entry = systems_.at(typeId);
        if (entry.enabled)
            entry.system->fixedUpdate(ctx, fixedDt);
    }
}

void SystemRegistry::runUpdate(AppContext& ctx, float dt) {
    for (const auto& typeId : phaseOrder_[static_cast<size_t>(SystemPhase::Update)]) {
        auto& entry = systems_.at(typeId);
        if (entry.enabled)
            entry.system->update(ctx, dt);
    }
}

void SystemRegistry::runPostUpdate(AppContext& ctx, float dt) {
    for (const auto& typeId : phaseOrder_[static_cast<size_t>(SystemPhase::PostUpdate)]) {
        auto& entry = systems_.at(typeId);
        if (entry.enabled)
            entry.system->update(ctx, dt);
    }
}

// PreRender, Render, PostRender call render(ctx)
void SystemRegistry::runPreRender(AppContext& ctx) {
    for (const auto& typeId : phaseOrder_[static_cast<size_t>(SystemPhase::PreRender)]) {
        auto& entry = systems_.at(typeId);
        if (entry.enabled)
            entry.system->render(ctx);
    }
}

void SystemRegistry::runRender(AppContext& ctx) {
    for (const auto& typeId : phaseOrder_[static_cast<size_t>(SystemPhase::Render)]) {
        auto& entry = systems_.at(typeId);
        if (entry.enabled)
            entry.system->render(ctx);
    }
}

void SystemRegistry::runPostRender(AppContext& ctx) {
    for (const auto& typeId : phaseOrder_[static_cast<size_t>(SystemPhase::PostRender)]) {
        auto& entry = systems_.at(typeId);
        if (entry.enabled)
            entry.system->render(ctx);
    }
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
