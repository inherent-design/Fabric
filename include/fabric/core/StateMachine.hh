#pragma once

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Utils.hh"
#include <algorithm>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace fabric {

// Generic parameterizable state machine with configurable transitions and hooks.
// Thread-safe via mutex. Self-transitions are no-ops.
template <typename StateEnum> class StateMachine {
  public:
    using Hook = std::function<void()>;
    using ToStringFn = std::function<std::string(StateEnum)>;

    StateMachine(StateEnum initialState, ToStringFn toStringFn)
        : currentState_(initialState), toStringFn_(std::move(toStringFn)) {}

    void addTransition(StateEnum from, StateEnum to) { transitions_.insert({from, to}); }

    void setState(StateEnum state) {
        std::vector<Hook> stateHooksToInvoke;
        std::vector<Hook> transHooksToInvoke;
        StateEnum oldState;

        {
            std::lock_guard<std::mutex> lock(stateMutex_);

            if (currentState_ == state) {
                return;
            }

            if (transitions_.count({currentState_, state}) == 0) {
                throwError("Invalid state transition from " + toStringFn_(currentState_) + " to " + toStringFn_(state));
            }

            oldState = currentState_;
            currentState_ = state;
        }

        FABRIC_LOG_DEBUG("State transition: {} -> {}", toStringFn_(oldState), toStringFn_(state));

        {
            std::lock_guard<std::mutex> lock(hooksMutex_);

            auto it = stateHooks_.find(state);
            if (it != stateHooks_.end()) {
                for (const auto& entry : it->second) {
                    stateHooksToInvoke.push_back(entry.hook);
                }
            }

            auto key = transitionKey(oldState, state);
            auto transIt = transitionHooks_.find(key);
            if (transIt != transitionHooks_.end()) {
                for (const auto& entry : transIt->second) {
                    transHooksToInvoke.push_back(entry.hook);
                }
            }
        }

        for (const auto& hook : stateHooksToInvoke) {
            try {
                hook();
            } catch (const std::exception& e) {
                FABRIC_LOG_ERROR("Exception in state hook: {}", e.what());
            } catch (...) {
                FABRIC_LOG_ERROR("Unknown exception in state hook");
            }
        }

        for (const auto& hook : transHooksToInvoke) {
            try {
                hook();
            } catch (const std::exception& e) {
                FABRIC_LOG_ERROR("Exception in transition hook: {}", e.what());
            } catch (...) {
                FABRIC_LOG_ERROR("Unknown exception in transition hook");
            }
        }
    }

    StateEnum getState() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return currentState_;
    }

    bool isValidTransition(StateEnum from, StateEnum to) const {
        if (from == to)
            return true;
        return transitions_.count({from, to}) > 0;
    }

    std::string addHook(StateEnum state, const Hook& hook) {
        if (!hook) {
            throwError("State hook cannot be null");
        }

        std::lock_guard<std::mutex> lock(hooksMutex_);
        auto id = Utils::generateUniqueId("hook_");
        stateHooks_[state].push_back(HookEntry{id, hook});
        FABRIC_LOG_DEBUG("Added state hook for '{}' with ID '{}'", toStringFn_(state), id);
        return id;
    }

    std::string addTransitionHook(StateEnum from, StateEnum to, const Hook& hook) {
        if (!hook) {
            throwError("Transition hook cannot be null");
        }

        std::lock_guard<std::mutex> lock(hooksMutex_);
        auto id = Utils::generateUniqueId("transition_");
        auto key = transitionKey(from, to);
        transitionHooks_[key].push_back(HookEntry{id, hook});
        FABRIC_LOG_DEBUG("Added transition hook from '{}' to '{}' with ID '{}'", toStringFn_(from), toStringFn_(to),
                         id);
        return id;
    }

    bool removeHook(const std::string& hookId) {
        std::lock_guard<std::mutex> lock(hooksMutex_);

        for (auto& [state, hooks] : stateHooks_) {
            auto it =
                std::find_if(hooks.begin(), hooks.end(), [&hookId](const HookEntry& e) { return e.id == hookId; });
            if (it != hooks.end()) {
                hooks.erase(it);
                FABRIC_LOG_DEBUG("Removed hook with ID '{}'", hookId);
                return true;
            }
        }

        for (auto& [key, hooks] : transitionHooks_) {
            auto it =
                std::find_if(hooks.begin(), hooks.end(), [&hookId](const HookEntry& e) { return e.id == hookId; });
            if (it != hooks.end()) {
                hooks.erase(it);
                FABRIC_LOG_DEBUG("Removed transition hook with ID '{}'", hookId);
                return true;
            }
        }

        return false;
    }

  private:
    struct HookEntry {
        std::string id;
        Hook hook;
    };

    static std::string transitionKey(StateEnum from, StateEnum to) {
        return std::to_string(static_cast<int>(from)) + ":" + std::to_string(static_cast<int>(to));
    }

    mutable std::mutex stateMutex_;
    StateEnum currentState_;
    ToStringFn toStringFn_;
    std::set<std::pair<StateEnum, StateEnum>> transitions_;

    mutable std::mutex hooksMutex_;
    std::unordered_map<StateEnum, std::vector<HookEntry>> stateHooks_;
    std::unordered_map<std::string, std::vector<HookEntry>> transitionHooks_;
};

} // namespace fabric
