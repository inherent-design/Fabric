#pragma once

#include "fabric/ui/RmlPanel.hh"
#include <cstddef>

namespace recurse {

struct LODStatsData {
    int pendingSections = 0;
    int gpuResidentSections = 0;
    int visibleSections = 0;
    float estimatedGpuMB = 0.0f;
};

class LODStatsPanel : public fabric::RmlPanel {
  public:
    LODStatsPanel() = default;
    ~LODStatsPanel() override = default;

    void init(Rml::Context* context);
    void update(const LODStatsData& data);

  private:
    int pendingSections_ = 0;
    int gpuResidentSections_ = 0;
    int visibleSections_ = 0;
    float estimatedGpuMB_ = 0.0f;
};

} // namespace recurse
