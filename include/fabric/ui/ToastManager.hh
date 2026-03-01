#pragma once

#include <string>
#include <vector>

namespace fabric {

/// Lightweight timed-message overlay.  Call show() to enqueue a toast,
/// update(dt) each frame, and query active()/currentMessage() to render.
class ToastManager {
  public:
    /// Display a message for `duration` seconds.
    void show(const std::string& message, float duration = 3.0f);

    /// Advance internal timers by `dt` seconds, expiring finished toasts.
    void update(float dt);

    /// True when at least one toast is visible.
    bool active() const;

    /// Return the most-recent active toast message (empty if none active).
    const std::string& currentMessage() const;

    /// Remove all pending toasts immediately.
    void clear();

  private:
    struct Toast {
        std::string message;
        float remaining = 0.0f;
    };

    std::vector<Toast> toasts_;
    static const std::string kEmpty_;
};

} // namespace fabric
