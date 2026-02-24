#pragma once

#include <RmlUi/Core/SystemInterface.h>

#include <chrono>

namespace fabric {

class BgfxSystemInterface : public Rml::SystemInterface {
  public:
    BgfxSystemInterface();

    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

  private:
    std::chrono::steady_clock::time_point startTime_;
};

} // namespace fabric
