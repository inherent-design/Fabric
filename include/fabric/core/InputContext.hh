#pragma once

#include "fabric/core/InputAction.hh"
#include "fabric/core/InputAxis.hh"
#include <string>
#include <unordered_map>
#include <vector>

namespace fabric {

/// An input context groups related action and axis bindings.
/// Contexts are stacked with priority ordering. Higher priority contexts
/// evaluate first and can consume input (preventing lower contexts from
/// seeing it).
///
/// Replaces the old InputMode enum. Each AppMode (Game, Paused, Console,
/// Menu, Editor) maps to one or more contexts pushed onto the stack.
class InputContext {
  public:
    explicit InputContext(std::string name, int priority = 0);

    const std::string& name() const;
    int priority() const;

    /// If true, inputs matched by this context are consumed (not passed
    /// to lower-priority contexts). Default true.
    bool consumeInput() const;
    void setConsumeInput(bool consume);

    /// If true, SDL events should be forwarded to RmlUI when this context
    /// is the topmost consuming context. Default false.
    bool routeToUI() const;
    void setRouteToUI(bool route);

    /// If false, this context is skipped during evaluation.
    bool enabled() const;
    void setEnabled(bool enabled);

    // --- Action bindings ---

    void addAction(const ActionBinding& binding);
    void removeAction(const std::string& actionName);
    const ActionBinding* findAction(const std::string& actionName) const;
    const std::vector<ActionBinding>& actions() const;

    // --- Axis bindings ---

    void addAxis(const AxisBinding& binding);
    void removeAxis(const std::string& axisName);
    const AxisBinding* findAxis(const std::string& axisName) const;
    const std::vector<AxisBinding>& axes() const;

    // --- Bulk operations ---

    /// Remove all bindings
    void clear();

    /// Replace all sources for a named action (for rebinding UI)
    bool rebindAction(const std::string& actionName, const std::vector<InputSource>& newSources);

    /// Replace all sources for a named axis (for rebinding UI)
    bool rebindAxis(const std::string& axisName, const std::vector<AxisSource>& newSources);

  private:
    std::string name_;
    int priority_ = 0;
    bool consumeInput_ = true;
    bool routeToUI_ = false;
    bool enabled_ = true;
    std::vector<ActionBinding> actions_;
    std::vector<AxisBinding> axes_;
    std::unordered_map<std::string, size_t> actionIndex_;
    std::unordered_map<std::string, size_t> axisIndex_;

    void rebuildActionIndex();
    void rebuildAxisIndex();
};

} // namespace fabric
