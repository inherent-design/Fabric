#pragma once

#include "fabric/ui/RmlPanel.hh"

namespace fabric {

struct ConcurrencyData {
    int activeWorkers = 0;
    int queuedJobs = 0;
};

class ConcurrencyPanel : public RmlPanel {
  public:
    ConcurrencyPanel() = default;
    ~ConcurrencyPanel() override = default;

    void init(Rml::Context* context);
    void update(const ConcurrencyData& data);

  private:
    int activeWorkers_ = 0;
    int queuedJobs_ = 0;
};

} // namespace fabric
