#pragma once

#include "fabric/core/AppModeManager.hh"
#include "fabric/ui/RmlPanel.hh"
#include <RmlUi/Core/Types.h>

namespace fabric {

struct WAILAData {
    bool hasHit = false;
    int voxelX = 0, voxelY = 0, voxelZ = 0;
    int chunkX = 0, chunkY = 0, chunkZ = 0;
    int normalX = 0, normalY = 0, normalZ = 0;
    float distance = 0.0f;
    float density = 0.0f;
    float essenceO = 0.0f, essenceC = 0.0f, essenceL = 0.0f, essenceD = 0.0f;
};

class WAILAPanel : public RmlPanel {
  public:
    WAILAPanel() = default;
    ~WAILAPanel() override = default;

    void init(Rml::Context* context);
    void update(const WAILAData& data);
    void setMode(AppMode mode);

  private:
    AppMode currentMode_ = AppMode::Game;

    bool hasHit_ = false;
    Rml::String displayText_;
    int voxelX_ = 0, voxelY_ = 0, voxelZ_ = 0;
    int chunkX_ = 0, chunkY_ = 0, chunkZ_ = 0;
    Rml::String normalStr_;
    float distance_ = 0.0f;
    float density_ = 0.0f;
    Rml::String essenceStr_;
};

} // namespace fabric
