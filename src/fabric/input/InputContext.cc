#include "fabric/input/InputContext.hh"

#include <algorithm>

namespace fabric {

InputContext::InputContext(std::string name, int priority) : name_(std::move(name)), priority_(priority) {}

const std::string& InputContext::name() const {
    return name_;
}

int InputContext::priority() const {
    return priority_;
}

bool InputContext::consumeInput() const {
    return consumeInput_;
}

void InputContext::setConsumeInput(bool consume) {
    consumeInput_ = consume;
}

bool InputContext::routeToUI() const {
    return routeToUI_;
}

void InputContext::setRouteToUI(bool route) {
    routeToUI_ = route;
}

bool InputContext::enabled() const {
    return enabled_;
}

void InputContext::setEnabled(bool enabled) {
    enabled_ = enabled;
}

// --- Actions ---

void InputContext::addAction(const ActionBinding& binding) {
    auto it = actionIndex_.find(binding.name);
    if (it != actionIndex_.end()) {
        actions_[it->second] = binding;
        return;
    }
    actionIndex_[binding.name] = actions_.size();
    actions_.push_back(binding);
}

void InputContext::removeAction(const std::string& actionName) {
    auto it = actionIndex_.find(actionName);
    if (it == actionIndex_.end())
        return;

    size_t idx = it->second;
    actions_.erase(actions_.begin() + static_cast<ptrdiff_t>(idx));
    rebuildActionIndex();
}

const ActionBinding* InputContext::findAction(const std::string& actionName) const {
    auto it = actionIndex_.find(actionName);
    if (it == actionIndex_.end())
        return nullptr;
    return &actions_[it->second];
}

const std::vector<ActionBinding>& InputContext::actions() const {
    return actions_;
}

// --- Axes ---

void InputContext::addAxis(const AxisBinding& binding) {
    axisIndex_[binding.name] = axes_.size();
    axes_.push_back(binding);
}

void InputContext::removeAxis(const std::string& axisName) {
    auto it = axisIndex_.find(axisName);
    if (it == axisIndex_.end())
        return;

    size_t idx = it->second;
    axes_.erase(axes_.begin() + static_cast<ptrdiff_t>(idx));
    rebuildAxisIndex();
}

const AxisBinding* InputContext::findAxis(const std::string& axisName) const {
    auto it = axisIndex_.find(axisName);
    if (it == axisIndex_.end())
        return nullptr;
    return &axes_[it->second];
}

const std::vector<AxisBinding>& InputContext::axes() const {
    return axes_;
}

// --- Bulk operations ---

void InputContext::clear() {
    actions_.clear();
    axes_.clear();
    actionIndex_.clear();
    axisIndex_.clear();
}

bool InputContext::rebindAction(const std::string& actionName, const std::vector<InputSource>& newSources) {
    auto it = actionIndex_.find(actionName);
    if (it == actionIndex_.end())
        return false;
    actions_[it->second].sources = newSources;
    return true;
}

bool InputContext::rebindAxis(const std::string& axisName, const std::vector<AxisSource>& newSources) {
    auto it = axisIndex_.find(axisName);
    if (it == axisIndex_.end())
        return false;
    axes_[it->second].sources = newSources;
    return true;
}

// --- Index rebuilds ---

void InputContext::rebuildActionIndex() {
    actionIndex_.clear();
    for (size_t i = 0; i < actions_.size(); ++i) {
        actionIndex_[actions_[i].name] = i;
    }
}

void InputContext::rebuildAxisIndex() {
    axisIndex_.clear();
    for (size_t i = 0; i < axes_.size(); ++i) {
        axisIndex_[axes_[i].name] = i;
    }
}

} // namespace fabric
