#include "fabric/ui/ToastManager.hh"

#include <algorithm>

namespace fabric {

const std::string ToastManager::kEmpty_;

void ToastManager::show(const std::string& message, float duration) {
    toasts_.push_back({message, duration});
}

void ToastManager::update(float dt) {
    for (auto& t : toasts_) {
        t.remaining -= dt;
    }
    std::erase_if(toasts_, [](const Toast& t) { return t.remaining <= 0.0f; });
}

bool ToastManager::active() const {
    return !toasts_.empty();
}

const std::string& ToastManager::currentMessage() const {
    if (toasts_.empty()) {
        return kEmpty_;
    }
    return toasts_.back().message;
}

void ToastManager::clear() {
    toasts_.clear();
}

} // namespace fabric
