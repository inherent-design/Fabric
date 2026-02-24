#include "fabric/core/Lifecycle.hh"
#include "fabric/core/Log.hh"

#include <set>

namespace fabric {

std::string lifecycleStateToString(LifecycleState state) {
    switch (state) {
        case LifecycleState::Created:
            return "Created";
        case LifecycleState::Initialized:
            return "Initialized";
        case LifecycleState::Rendered:
            return "Rendered";
        case LifecycleState::Updating:
            return "Updating";
        case LifecycleState::Suspended:
            return "Suspended";
        case LifecycleState::Destroyed:
            return "Destroyed";
        default:
            return "Unknown";
    }
}

namespace {

// Single source of truth for lifecycle transitions, used by both the
// constructor (to configure the StateMachine) and the static isValidTransition.
const auto& lifecycleTransitions() {
    static const std::set<std::pair<LifecycleState, LifecycleState>> t = {
        {LifecycleState::Created, LifecycleState::Initialized},
        {LifecycleState::Created, LifecycleState::Destroyed},
        {LifecycleState::Initialized, LifecycleState::Rendered},
        {LifecycleState::Initialized, LifecycleState::Suspended},
        {LifecycleState::Initialized, LifecycleState::Destroyed},
        {LifecycleState::Rendered, LifecycleState::Updating},
        {LifecycleState::Rendered, LifecycleState::Suspended},
        {LifecycleState::Rendered, LifecycleState::Destroyed},
        {LifecycleState::Updating, LifecycleState::Rendered},
        {LifecycleState::Updating, LifecycleState::Suspended},
        {LifecycleState::Updating, LifecycleState::Destroyed},
        {LifecycleState::Suspended, LifecycleState::Initialized},
        {LifecycleState::Suspended, LifecycleState::Rendered},
        {LifecycleState::Suspended, LifecycleState::Destroyed},
    };
    return t;
}

} // anonymous namespace

LifecycleManager::LifecycleManager() : sm_(LifecycleState::Created, lifecycleStateToString) {
    for (const auto& [from, to] : lifecycleTransitions()) {
        sm_.addTransition(from, to);
    }
}

void LifecycleManager::setState(LifecycleState state) {
    auto previous = sm_.getState();
    sm_.setState(state);
    FABRIC_LOG_DEBUG("Lifecycle: transition {} -> {}", lifecycleStateToString(previous), lifecycleStateToString(state));
}

LifecycleState LifecycleManager::getState() const {
    return sm_.getState();
}

std::string LifecycleManager::addHook(LifecycleState state, const LifecycleHook& hook) {
    return sm_.addHook(state, hook);
}

std::string LifecycleManager::addTransitionHook(LifecycleState fromState, LifecycleState toState,
                                                const LifecycleHook& hook) {
    return sm_.addTransitionHook(fromState, toState, hook);
}

bool LifecycleManager::removeHook(const std::string& hookId) {
    return sm_.removeHook(hookId);
}

bool LifecycleManager::isValidTransition(LifecycleState fromState, LifecycleState toState) {
    if (fromState == toState)
        return true;
    return lifecycleTransitions().count({fromState, toState}) > 0;
}

} // namespace fabric
