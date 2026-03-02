#include "fabric/core/SystemPhase.hh"

namespace fabric {

std::string systemPhaseToString(SystemPhase phase) {
    switch (phase) {
        case SystemPhase::PreUpdate:
            return "PreUpdate";
        case SystemPhase::FixedUpdate:
            return "FixedUpdate";
        case SystemPhase::Update:
            return "Update";
        case SystemPhase::PostUpdate:
            return "PostUpdate";
        case SystemPhase::PreRender:
            return "PreRender";
        case SystemPhase::Render:
            return "Render";
        case SystemPhase::PostRender:
            return "PostRender";
    }
    return "Unknown";
}

} // namespace fabric
