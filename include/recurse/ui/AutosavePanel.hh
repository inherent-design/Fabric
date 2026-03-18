#pragma once

#include "fabric/ui/RmlPanel.hh"

namespace recurse {

struct AutosaveIndicatorData {
    bool visible = false;
    Rml::String statusText;
    Rml::String detailText;
};

class AutosavePanel : public fabric::RmlPanel {
  public:
    void init(Rml::Context* context);
    void update(const AutosaveIndicatorData& data);

  private:
    Rml::String statusText_;
    Rml::String detailText_;
};

} // namespace recurse
