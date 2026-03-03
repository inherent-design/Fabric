#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
class ElementDocument;
} // namespace Rml

namespace fabric {

/// Data collected from a per-frame crosshair raycast.
struct WAILAData {
    bool hasHit = false;
    int voxelX = 0, voxelY = 0, voxelZ = 0;
    int chunkX = 0, chunkY = 0, chunkZ = 0;
    int normalX = 0, normalY = 0, normalZ = 0;
    float distance = 0.0f;
    float density = 0.0f;
    float essenceR = 0.0f, essenceG = 0.0f, essenceB = 0.0f;
};

/// "What Am I Looking At" info panel. Displays crosshair target data
/// (position, density, essence color) from a per-frame voxel raycast.
/// Toggle visibility alongside the debug HUD (F3).
class WAILAPanel {
  public:
    WAILAPanel() = default;
    ~WAILAPanel() = default;

    void init(Rml::Context* context);
    void update(const WAILAData& data);
    void toggle();
    void shutdown();
    bool isVisible() const;

  private:
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    bool visible_ = false;
    bool initialized_ = false;

    // Bound variables for the "waila" data model
    bool hasHit_ = false;
    Rml::String displayText_;
    int voxelX_ = 0, voxelY_ = 0, voxelZ_ = 0;
    int chunkX_ = 0, chunkY_ = 0, chunkZ_ = 0;
    Rml::String normalStr_;
    float distance_ = 0.0f;
    float density_ = 0.0f;
    Rml::String essenceStr_;

    Rml::DataModelHandle modelHandle_;
};

} // namespace fabric
