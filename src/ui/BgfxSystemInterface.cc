#include "fabric/ui/BgfxSystemInterface.hh"
#include "fabric/core/Log.hh"

namespace fabric {

BgfxSystemInterface::BgfxSystemInterface() : startTime_(std::chrono::steady_clock::now()) {}

double BgfxSystemInterface::GetElapsedTime() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - startTime_).count();
}

bool BgfxSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
    switch (type) {
        case Rml::Log::LT_ERROR:
        case Rml::Log::LT_ASSERT:
            FABRIC_LOG_ERROR("[RmlUi] {}", message);
            break;
        case Rml::Log::LT_WARNING:
            FABRIC_LOG_WARN("[RmlUi] {}", message);
            break;
        case Rml::Log::LT_INFO:
            FABRIC_LOG_INFO("[RmlUi] {}", message);
            break;
        case Rml::Log::LT_DEBUG:
        case Rml::Log::LT_MAX:
        case Rml::Log::LT_ALWAYS:
            FABRIC_LOG_DEBUG("[RmlUi] {}", message);
            break;
    }
    return true;
}

} // namespace fabric
